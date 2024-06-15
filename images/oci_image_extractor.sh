#!/bin/bash

OUTPUT_DIR=$1
OCI_IMAGE_DIR=$2

if [ -d $OUTPUT_DIR ]; then 
	rm -rf $OUTPUT_DIR
fi

echo "Changing dir to $OCI_IMAGE_DIR"
cd $OCI_IMAGE_DIR
mkdir -p $OUTPUT_DIR

MANIFEST_DIGEST=$(jq '.manifests[] | select(.mediaType != null) | select(.mediaType == "application/vnd.oci.image.manifest.v1+json") | .digest' index.json | tr -d \" | cut -d':' -f2)
BLOBS_DIR="blobs/$(jq '.manifests[] | select(.mediaType != null) | select(.mediaType == "application/vnd.oci.image.manifest.v1+json") | .digest' index.json | tr -d \" | cut -d':' -f1)"

echo "manifest.json is at $BLOBS_DIR/$MANIFEST_DIGEST"
cd $BLOBS_DIR
LAYERS=$(jq '.layers[] | .digest' $MANIFEST_DIGEST | tr -d \" | cut -d':' -f2)
echo "Layers are: $LAYERS"

echo "Extracting layers to $OUTPUT_DIR"
for LAYER in $LAYERS
do
	cp $LAYER $OUTPUT_DIR/$LAYER.tgz
	tar --overwrite -xzf $OUTPUT_DIR/$LAYER.tgz -C $OUTPUT_DIR
	rm $OUTPUT_DIR/$LAYER.tgz
	echo $LAYER
done
