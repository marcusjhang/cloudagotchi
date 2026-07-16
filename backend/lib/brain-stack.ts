import * as cdk from 'aws-cdk-lib';
import * as dynamodb from 'aws-cdk-lib/aws-dynamodb';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import * as iot from 'aws-cdk-lib/aws-iot';
import * as iam from 'aws-cdk-lib/aws-iam';
import * as scheduler from 'aws-cdk-lib/aws-scheduler';
import { Construct } from 'constructs';
import * as path from 'path';

/**
 * The pet's brain (article 3):
 *
 *   interactions:  IoT rule → onInteraction Lambda → DynamoDB → MQTT state back
 *   time passing:  EventBridge Scheduler (every 30 min) → decay Lambda → same
 */
export class BrainStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    // One row per pet. That's the whole database.
    const table = new dynamodb.Table(this, 'PetState', {
      tableName: 'cloudagotchi-pets',
      partitionKey: { name: 'deviceId', type: dynamodb.AttributeType.STRING },
      billingMode: dynamodb.BillingMode.PAY_PER_REQUEST,
      removalPolicy: cdk.RemovalPolicy.DESTROY, // it's a pet, not a bank
    });

    // Lambdas may publish state updates back to the device.
    const iotPublishPolicy = new iam.PolicyStatement({
      actions: ['iot:Publish'],
      resources: [`arn:aws:iot:${this.region}:${this.account}:topic/cloudagotchi/*/state`],
    });

    const commonEnv = {
      TABLE_NAME: table.tableName,
      IOT_ENDPOINT: this.node.tryGetContext('iotEndpoint') ?? '',
    };

    // Reacts to feed / pet / play / hello messages from the device.
    const onInteraction = new lambda.Function(this, 'OnInteraction', {
      runtime: lambda.Runtime.NODEJS_22_X,
      handler: 'on-interaction.handler',
      code: lambda.Code.fromAsset(path.join(__dirname, '../lambda/brain')),
      environment: commonEnv,
      timeout: cdk.Duration.seconds(10),
    });

    // Makes time pass: stats decay even when the device is off.
    const decay = new lambda.Function(this, 'Decay', {
      runtime: lambda.Runtime.NODEJS_22_X,
      handler: 'decay.handler',
      code: lambda.Code.fromAsset(path.join(__dirname, '../lambda/brain')),
      environment: commonEnv,
      timeout: cdk.Duration.seconds(30),
    });

    table.grantReadWriteData(onInteraction);
    table.grantReadWriteData(decay);
    onInteraction.addToRolePolicy(iotPublishPolicy);
    decay.addToRolePolicy(iotPublishPolicy);

    // IoT rule: every device→cloud message lands in the interaction Lambda.
    // The SQL grabs the deviceId straight out of the topic path.
    new iot.CfnTopicRule(this, 'InteractionRule', {
      ruleName: 'cloudagotchi_interactions',
      topicRulePayload: {
        sql: `SELECT *, topic(2) AS deviceId, topic(3) AS kind
              FROM 'cloudagotchi/+/interaction'`,
        awsIotSqlVersion: '2016-03-23',
        actions: [{ lambda: { functionArn: onInteraction.functionArn } }],
      },
    });

    new iot.CfnTopicRule(this, 'HelloRule', {
      ruleName: 'cloudagotchi_hello',
      topicRulePayload: {
        sql: `SELECT *, topic(2) AS deviceId, topic(3) AS kind
              FROM 'cloudagotchi/+/hello'`,
        awsIotSqlVersion: '2016-03-23',
        actions: [{ lambda: { functionArn: onInteraction.functionArn } }],
      },
    });

    // IoT needs explicit permission to invoke the Lambda.
    onInteraction.addPermission('AllowIotInvoke', {
      principal: new iam.ServicePrincipal('iot.amazonaws.com'),
      sourceArn: `arn:aws:iot:${this.region}:${this.account}:rule/*`,
    });

    // Time passes every 30 minutes, whether you're watching or not.
    const schedulerRole = new iam.Role(this, 'SchedulerRole', {
      assumedBy: new iam.ServicePrincipal('scheduler.amazonaws.com'),
    });
    decay.grantInvoke(schedulerRole);

    new scheduler.CfnSchedule(this, 'DecaySchedule', {
      scheduleExpression: 'rate(30 minutes)',
      flexibleTimeWindow: { mode: 'OFF' },
      target: {
        arn: decay.functionArn,
        roleArn: schedulerRole.roleArn,
      },
    });
  }
}
