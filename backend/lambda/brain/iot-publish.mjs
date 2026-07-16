// Push a pet's state down to its device over MQTT.
import { IoTDataPlaneClient, PublishCommand } from '@aws-sdk/client-iot-data-plane';
import { moodOf } from './pet-logic.mjs';

// Use the account's ATS data endpoint — the SDK's default endpoint is the
// legacy (non-ATS) one and can fail TLS verification in some regions.
const iot = new IoTDataPlaneClient(
  process.env.IOT_ENDPOINT ? { endpoint: `https://${process.env.IOT_ENDPOINT}` } : {}
);

export async function publishState(pet) {
  const state = {
    hunger: pet.hunger,
    energy: pet.energy,
    mood: pet.mood,
    face: moodOf(pet, new Date().getUTCHours()),
  };

  await iot.send(new PublishCommand({
    topic: `cloudagotchi/${pet.deviceId}/state`,
    qos: 1,
    payload: JSON.stringify(state),
  }));

  return state;
}
