FROM scratch

ADD test.txt /test.txt
ADD user/_sh user/_pouch user/_ls user/_cat user/_echo /

# Squash all layers to a single layer
FROM scratch
COPY --from=0 /* /