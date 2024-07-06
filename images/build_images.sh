#!/bin/bash

# Iterate through img_internal_fs_[abc].Dockerfile, build and save as OCI.
for i in a b c
do
    # Build the image using Docker
    docker build -t img_internal_fs_$i -f build/img_internal_fs_$i.Dockerfile build

    # Convert the image to OCI format in the img_internal_fs_$i directory.
    skopeo copy docker-daemon:img_internal_fs_$i:latest oci:images/img_internal_fs_$i
done
