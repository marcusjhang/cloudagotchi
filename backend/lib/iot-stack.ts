import * as cdk from 'aws-cdk-lib';
import * as iot from 'aws-cdk-lib/aws-iot';
import { Construct } from 'constructs';

/**
 * The pet's nervous system: an AWS IoT policy scoped so that a device
 * can only ever talk on its *own* topics (cloudagotchi/<thingName>/...).
 *
 * The certificate + thing are created per-device by scripts/provision-device.sh,
 * because certificates are secrets and don't belong in CloudFormation.
 */
export class IotStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    const region = this.region;
    const account = this.account;

    new iot.CfnPolicy(this, 'CloudagotchiDevicePolicy', {
      policyName: 'CloudagotchiDevicePolicy',
      policyDocument: {
        Version: '2012-10-17',
        Statement: [
          {
            // A device may connect only with its own thing name as client id.
            Effect: 'Allow',
            Action: 'iot:Connect',
            Resource: `arn:aws:iot:${region}:${account}:client/\${iot:Connection.Thing.ThingName}`,
          },
          {
            // Device → cloud: interactions, boot hello.
            Effect: 'Allow',
            Action: 'iot:Publish',
            Resource: `arn:aws:iot:${region}:${account}:topic/cloudagotchi/\${iot:Connection.Thing.ThingName}/*`,
          },
          {
            // Cloud → device: state updates, briefings.
            Effect: 'Allow',
            Action: 'iot:Subscribe',
            Resource: `arn:aws:iot:${region}:${account}:topicfilter/cloudagotchi/\${iot:Connection.Thing.ThingName}/*`,
          },
          {
            Effect: 'Allow',
            Action: 'iot:Receive',
            Resource: `arn:aws:iot:${region}:${account}:topic/cloudagotchi/\${iot:Connection.Thing.ThingName}/*`,
          },
        ],
      },
    });

    new cdk.CfnOutput(this, 'DevicePolicyName', {
      value: 'CloudagotchiDevicePolicy',
      description: 'Attach this IoT policy to each device certificate',
    });
  }
}
