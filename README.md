# ReadMpls

Get m2ts clip id from a playlist and return a dict.

There are three items in the dict:
- 'count' contains the clip counts in the playlist.
- 'clip' contains a list of full paths to each m2ts file in the playlist.
- 'filename' contains a list of file names of each m2ts file in the playlist.


## Parameters

```py
mpls.Read(string bd_path, int playlist[, int angle=0])
```

- bd_path: Full path to the root of Blu-ray disc or directory. Don't use relative path and don't contain a trailing slash at the end.

- playlist: Playlist number, which is the number in mpls file name.

- angle: Angle number to select in the playlist.

After obtaining the dict, you can use your favorite source filter to open them all with a for-loop and splice them together. For example:

```py
mpls = core.mpls.Read(r'D:\Downloads\rule6', 0)
clip = core.std.Splice([core.bs.VideoSource(mpls['clip'][i]) for i in range(mpls['count'])])
```


## Installation

```
pip install -U vapoursynth-readmpls
```
