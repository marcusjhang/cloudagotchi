// Fires for every device→cloud message (IoT rule on interaction + hello topics).
import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, GetCommand, PutCommand } from '@aws-sdk/lib-dynamodb';
import { newbornPet, applyDecay, applyInteraction } from './pet-logic.mjs';
import { publishState } from './iot-publish.mjs';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const TABLE = process.env.TABLE_NAME;

export const handler = async (event) => {
  // The IoT rule SQL injected deviceId (from the topic) into the payload.
  const { deviceId, kind, type } = event;
  const now = Date.now();

  // 1. Load the pet — or meet it for the first time.
  const { Item } = await ddb.send(new GetCommand({ TableName: TABLE, Key: { deviceId } }));
  let pet = Item ?? newbornPet(deviceId, now);

  // 2. Time passed since we last looked. It always does.
  pet = applyDecay(pet, now);

  // 3. A "hello" just wants the current state; an interaction changes it.
  if (kind === 'interaction') {
    pet = applyInteraction(pet, type);
    console.log(`${deviceId}: ${type} → hunger=${pet.hunger} energy=${pet.energy} mood=${pet.mood}`);
  }

  // 4. Persist, then tell the device what it now feels.
  await ddb.send(new PutCommand({ TableName: TABLE, Item: pet }));
  await publishState(pet);
};
