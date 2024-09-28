FROM scratch

ADD hello.txt /hello.txt
ADD user/_sh ./sh
ADD user/_pouch ./pouch

# Squash all layers to a single layer
FROM scratch
COPY --from=0 /* /