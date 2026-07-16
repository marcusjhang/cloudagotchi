// Fires every 30 minutes (EventBridge Scheduler): time passes for every pet.
import { DynamoDBClient } from '@aws-sdk/client-dynamodb';
import { DynamoDBDocumentClient, ScanCommand, PutCommand } from '@aws-sdk/lib-dynamodb';
import { applyDecay } from './pet-logic.mjs';
import { publishState } from './iot-publish.mjs';

const ddb = DynamoDBDocumentClient.from(new DynamoDBClient({}));
const TABLE = process.env.TABLE_NAME;

export const handler = async () => {
  // A Scan is a sin at scale — and perfectly fine for a hobby fleet of pets.
  const { Items = [] } = await ddb.send(new ScanCommand({ TableName: TABLE }));
  const now = Date.now();

  for (const pet of Items) {
    const aged = applyDecay(pet, now);
    await ddb.send(new PutCommand({ TableName: TABLE, Item: aged }));
    await publishState(aged); // if the device is offline, IoT just drops it —
                              // it will ask again via "hello" on next boot.
  }

  console.log(`Time passed for ${Items.length} pet(s)`);
};
