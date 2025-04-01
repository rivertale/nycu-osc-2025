#!/bin/sh

cd "$(dirname "$0")"
image_name="osc"
container_name="ct${image_name}"


# build docker if needed
docker inspect --type=image "${image_name}" 1>/dev/null 2>/dev/null
if [ $? -ne 0 ]
then
    echo "Unable to find docker image '${image_name}', try to build it"
    docker build -t "${image_name}" .
    if [ $? -eq 0 ]; then echo "Docker image '${image_name}' built"; fi
fi


# parse arguments
args=""
for arg in "$@"
do
    args="${args} \"${arg}\""
done


# build
cd ..
docker exec --interactive --tty --privileged \
    --workdir "$(pwd)/code" \
    "${container_name}" ./debug.sh
