# Movie statistics calculator

When a certain compression is applied to the frame of a video, we can use the [SSIM](https://en.wikipedia.org/wiki/Structural_similarity) to know how much the compression deteriorated the frame.

This program is able to obtain the ssim of a each frame of a video at various compression (using [ffmpeg](https://ffmpeg.org)), and compute mean, variance and autocorrelation of this timeseries.

Insert the videos you want to know the statistics of in the folder "Videos".

## Build
docker build -t stats .

## Run
docker run -it stats make run
