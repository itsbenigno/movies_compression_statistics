# guide

docker build -t stats .

docker run -it stats ./program

## dev with docker
docker run -it --mount type=bind,source=$(pwd),target="/app" stats
