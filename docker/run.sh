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
docker run --rm --interactive --tty --privileged --security-opt seccomp=unconfined \
    --volume "$(pwd):$(pwd)" \
    --workdir "$(pwd)/code" \
    --name "${container_name}" \
    "${image_name}" sh -c "
        groupadd --gid $(id -g) \"g${image_name}\"
        useradd --uid $(id -u) --gid $(id -g) \"u${image_name}\"
        exec sudo --user=\"u${image_name}\" ./run.sh
  "
