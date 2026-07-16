#!/usr/bin/env node
import * as cdk from 'aws-cdk-lib';
import { IotStack } from '../lib/iot-stack';
import { BrainStack } from '../lib/brain-stack';
import { NewsStack } from '../lib/news-stack';

const app = new cdk.App();

// Article 1 — the pet's nervous system: device identity & MQTT permissions.
new IotStack(app, 'CloudagotchiIotStack');

// Article 3 — the pet's brain: state, interactions, and the passage of time.
new BrainStack(app, 'CloudagotchiBrainStack');

// Article 4 — the pet's job: the morning AWS news briefing.
new NewsStack(app, 'CloudagotchiNewsStack');
