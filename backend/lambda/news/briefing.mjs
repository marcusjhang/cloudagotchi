// The pet's morning job: fetch AWS news, summarize them in its own voice,
// give the words a squeaky voice with Polly, and deliver the paper over MQTT.
import { BedrockRuntimeClient, ConverseCommand } from '@aws-sdk/client-bedrock-runtime';
import { PollyClient, SynthesizeSpeechCommand } from '@aws-sdk/client-polly';
import { S3Client, PutObjectCommand, GetObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';
import { IoTClient, ListThingsCommand } from '@aws-sdk/client-iot';
import { IoTDataPlaneClient, PublishCommand } from '@aws-sdk/client-iot-data-plane';
import { fetchNews } from './rss.mjs';

const bedrock = new BedrockRuntimeClient({});
const polly = new PollyClient({});
const s3 = new S3Client({});
const iot = new IoTClient({});
// Use the account's ATS data endpoint — the SDK default is the legacy one.
const iotData = new IoTDataPlaneClient(
  process.env.IOT_ENDPOINT ? { endpoint: `https://${process.env.IOT_ENDPOINT}` } : {}
);

const BUCKET = process.env.BUCKET_NAME;
const MODEL = process.env.BEDROCK_MODEL_ID;
const VOICE = process.env.POLLY_VOICE_ID;

const PERSONA = `You are Cloudagotchi, a small, excitable ghost virtual pet who reads
the morning AWS news to your human. You are affectionate, easily impressed, and
sometimes admit you don't fully understand the more complicated services.
Summarize the following AWS announcements as a spoken morning briefing:
- 45 to 60 seconds when read aloud (about 120-150 words)
- Start with a cheerful good-morning in character
- Pick only the 3 most interesting items, one short sentence of WHY each matters
- End with one affectionate sign-off sentence
- Plain text only: no emoji, no markdown, no bullet points (it will be read aloud)`;

export const handler = async () => {
  // 1. Fetch the latest AWS "What's New" items.
  const items = await fetchNews(10);
  console.log(`Fetched ${items.length} news items`);

  // 2. Bedrock turns announcements into the pet's morning monologue.
  const { output } = await bedrock.send(new ConverseCommand({
    modelId: MODEL,
    messages: [{
      role: 'user',
      content: [{ text: `${PERSONA}\n\nToday's announcements:\n${items.map(i => `- ${i.title}`).join('\n')}` }],
    }],
    inferenceConfig: { maxTokens: 500, temperature: 0.8 },
  }));
  const script = output.message.content[0].text;
  console.log(`Briefing script:\n${script}`);

  // 3. Polly speaks it — pitched up, because it's a small ghost.
  //    PCM output (not MP3!) so the ESP32 can play it with zero decoding.
  const { AudioStream } = await polly.send(new SynthesizeSpeechCommand({
    Text: `<speak><prosody pitch="+20%" rate="105%">${escapeXml(script)}</prosody></speak>`,
    TextType: 'ssml',
    VoiceId: VOICE,
    OutputFormat: 'pcm',       // raw signed 16-bit little-endian mono
    SampleRate: '16000',
  }));
  const pcm = Buffer.from(await AudioStream.transformToByteArray());

  // 4. Wrap the raw PCM in a WAV header and park it in S3.
  const wav = pcmToWav(pcm, 16000);
  const key = `briefings/${new Date().toISOString().slice(0, 10)}.wav`;
  await s3.send(new PutObjectCommand({ Bucket: BUCKET, Key: key, Body: wav, ContentType: 'audio/wav' }));

  // 5. A presigned URL lets the device download with plain HTTPS — no AWS
  //    credentials on the ESP32 beyond its IoT certificate.
  const url = await getSignedUrl(s3, new GetObjectCommand({ Bucket: BUCKET, Key: key }),
    { expiresIn: 12 * 3600 });

  // 6. Ring every pet's doorbell: "the paper is here".
  const { things = [] } = await iot.send(new ListThingsCommand({ maxResults: 50 }));
  const headline = items[0]?.title ?? 'AWS news';
  for (const thing of things) {
    if (!thing.thingName.startsWith('cloudagotchi')) continue;
    await iotData.send(new PublishCommand({
      topic: `cloudagotchi/${thing.thingName}/briefing`,
      qos: 1,
      payload: JSON.stringify({ url, headline, bytes: wav.length }),
    }));
  }

  return { ok: true, items: items.length, bytes: wav.length };
};

function escapeXml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

/** Minimal WAV header for 16-bit mono PCM — 44 bytes of 1991 technology. */
function pcmToWav(pcm, sampleRate) {
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(36 + pcm.length, 4);
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16);            // PCM chunk size
  header.writeUInt16LE(1, 20);             // audio format: PCM
  header.writeUInt16LE(1, 22);             // channels: mono
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(sampleRate * 2, 28); // byte rate (16-bit mono)
  header.writeUInt16LE(2, 32);             // block align
  header.writeUInt16LE(16, 34);            // bits per sample
  header.write('data', 36);
  header.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([header, pcm]);
}
