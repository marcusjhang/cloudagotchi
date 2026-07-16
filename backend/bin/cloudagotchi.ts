#!/usr/bin/env node
import * as cdk from 'aws-cdk-lib';
import { IotStack } from '../lib/iot-stack';

const app = new cdk.App();

// Article 1 — the pet's nervous system: device identity & MQTT permissions.
new IotStack(app, 'CloudagotchiIotStack');
