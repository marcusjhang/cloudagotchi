import * as cdk from 'aws-cdk-lib';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import * as s3 from 'aws-cdk-lib/aws-s3';
import * as iam from 'aws-cdk-lib/aws-iam';
import * as scheduler from 'aws-cdk-lib/aws-scheduler';
import { Construct } from 'constructs';
import * as path from 'path';

/**
 * The pet's job (article 4): every morning,
 *
 *   AWS "What's New" RSS → Bedrock (summary, in the pet's voice)
 *                        → Polly (squeaky text-to-speech, WAV)
 *                        → S3 (audio file, presigned URL)
 *                        → MQTT nudge → device plays it on the speaker
 */
export class NewsStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    const bucket = new s3.Bucket(this, 'Briefings', {
      removalPolicy: cdk.RemovalPolicy.DESTROY,
      autoDeleteObjects: true,
      lifecycleRules: [{ expiration: cdk.Duration.days(7) }], // old news is old
    });

    const briefing = new lambda.Function(this, 'MorningBriefing', {
      runtime: lambda.Runtime.NODEJS_22_X,
      handler: 'briefing.handler',
      code: lambda.Code.fromAsset(path.join(__dirname, '../lambda/news')),
      timeout: cdk.Duration.minutes(2),
      memorySize: 512,
      environment: {
        BUCKET_NAME: bucket.bucketName,
        IOT_ENDPOINT: this.node.tryGetContext('iotEndpoint') ?? '',
        BEDROCK_MODEL_ID: 'us.anthropic.claude-haiku-4-5-20251001-v1:0',
        POLLY_VOICE_ID: 'Justin', // young-sounding; we pitch it up with SSML
      },
    });

    bucket.grantReadWrite(briefing);

    briefing.addToRolePolicy(new iam.PolicyStatement({
      actions: ['bedrock:InvokeModel'],
      // Inference profiles route across regions, so both the profile ARN
      // and the underlying foundation models must be allowed.
      resources: [
        `arn:aws:bedrock:${this.region}:${this.account}:inference-profile/*`,
        'arn:aws:bedrock:*::foundation-model/*',
      ],
    }));
    briefing.addToRolePolicy(new iam.PolicyStatement({
      actions: ['polly:SynthesizeSpeech'],
      resources: ['*'], // SynthesizeSpeech does not support resource scoping
    }));
    briefing.addToRolePolicy(new iam.PolicyStatement({
      actions: ['iot:Publish'],
      resources: [`arn:aws:iot:${this.region}:${this.account}:topic/cloudagotchi/*/briefing`],
    }));
    briefing.addToRolePolicy(new iam.PolicyStatement({
      actions: ['iot:ListThings'],
      resources: ['*'], // ListThings does not support resource scoping
    }));

    // The paper arrives every morning at 7:03 (pet's local time — adjust!).
    const schedulerRole = new iam.Role(this, 'SchedulerRole', {
      assumedBy: new iam.ServicePrincipal('scheduler.amazonaws.com'),
    });
    briefing.grantInvoke(schedulerRole);

    new scheduler.CfnSchedule(this, 'MorningSchedule', {
      scheduleExpression: 'cron(3 7 * * ? *)',
      scheduleExpressionTimezone: 'Europe/Paris',
      flexibleTimeWindow: { mode: 'OFF' },
      target: { arn: briefing.functionArn, roleArn: schedulerRole.roleArn },
    });
  }
}
