# Updating Screenshots

The screenshots in this directory were taken from my watch using the `rebble`
tool.  The screenshot in this directory was created using a command like:

`rebble screenshot --phone 192.168.178.192`

I also created [a timelapse movie as well](screenshot.mp4).  For this, I first
took 60 frames a second apart, using a shell command like:

```
for ((i=10; i<70; i++)); do rebble screenshot --phone 192.168.178.192 frames/frame-$i.png; sleep 1; done
```

I used 10 as the starting frame so that they would be sorted properly for the next step.  Once those files are created, you can use `ffmpeg` to put them together into a movie using a command like:

```
ffmpeg -framerate 25 -pattern_type glob -i 'frames/*.png' screenshot.mp4 
``
