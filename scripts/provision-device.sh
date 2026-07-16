#!/usr/bin/env bash
#
# Provision one Cloudagotchi device:
#   - creates an IoT Thing
#   - creates a certificate + private key
#   - attaches the CloudagotchiDevicePolicy (deployed by the CDK stack)
#   - drops the credentials where the firmware build expects them
#
# Usage: ./scripts/provision-device.sh cloudagotchi-01
set -euo pipefail

THING_NAME="${1:?Usage: $0 <thing-name>   e.g. $0 cloudagotchi-01}"
CERTS_DIR="$(dirname "$0")/../firmware/main/certs"
mkdir -p "$CERTS_DIR"

echo "→ Creating IoT Thing '$THING_NAME'..."
aws iot create-thing --thing-name "$THING_NAME" >/dev/null

echo "→ Creating certificate + private key..."
CERT_ARN=$(aws iot create-keys-and-certificate \
  --set-as-active \
  --certificate-pem-outfile "$CERTS_DIR/device-cert.pem" \
  --private-key-outfile "$CERTS_DIR/device-key.pem" \
  --query 'certificateArn' --output text)

echo "→ Attaching policy and thing to certificate..."
aws iot attach-policy --policy-name CloudagotchiDevicePolicy --target "$CERT_ARN"
aws iot attach-thing-principal --thing-name "$THING_NAME" --principal "$CERT_ARN"

echo "→ Downloading Amazon Root CA..."
curl -s https://www.amazontrust.com/repository/AmazonRootCA1.pem \
  -o "$CERTS_DIR/AmazonRootCA1.pem"

ENDPOINT=$(aws iot describe-endpoint --endpoint-type iot:Data-ATS \
  --query 'endpointAddress' --output text)

echo ""
echo "✅ Device provisioned!"
echo "   Credentials written to firmware/main/certs/"
echo ""
echo "   Now configure the firmware:"
echo "     cd firmware && idf.py menuconfig"
echo "       → Cloudagotchi Configuration"
echo "         → AWS IoT endpoint: $ENDPOINT"
echo "         → Thing name:       $THING_NAME"
