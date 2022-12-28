# guide

docker build -t ubuntu .

docker run -it ubuntu ./program

## dev with docker
docker run -it --mount type=bind,source=$(pwd),target="/app" ubuntu
