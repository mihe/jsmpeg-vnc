# jsmpeg-vnc

A low latency, high framerate screen sharing server and client, viewable in any modern browser.

This fork replaces the original GDI screen capturing with a Desktop Duplication API implementation, which greatly reduces the performance impact of running the application. It also fixes some minor issues with the WebSocket implementation. I've also (unfortunately) removed the remote input functionality due to not really needing it myself.

## Usage

```
jsmpeg-vnc.exe {OPTIONS}

OPTIONS:
    -h, --help                        Display this help menu
    -p[port], --port=[port]           Server port (default: 8081)
    -x[width], --width=[width]        Width of the stream output (default: [screen width])
    -y[height], --height=[height]     Height of the stream output (default: [screen height])
    -b[bitrate], --bitrate=[bitrate]  Average bitrate of stream output
    -r[fps], --fps=[fps]              Target framerate of stream output (default: 30)
    -t[threads], --threads=[threads]  Number of threads to use for encoding (default: 2)
    -o[output], --output=[output]     Index of the output adapter (default: 0)

Example:
    jsmpeg-vnc.exe -p 8080 -b 2000 -r 30
```	

## Technology & License

This app uses [ffmpeg][ffmpeg] for encoding, [libwebsockets][lws] for the WebSocket server and [jsmpeg][jsmpeg] for decoding in the browser. Note that the jsmpeg version in this repository has been modified to get rid of an extra frame of latency. The server sends each frame with a custom header, so the resulting WebSocket stream is not a valid MPEG video anymore.

jsmpeg-vnc is published under the [GPLv3 License][gpl].

[ffmpeg]: https://github.com/FFmpeg/FFmpeg
[lws]: https://github.com/warmcat/libwebsockets
[jsmpeg]: https://github.com/phoboslab/jsmpeg
[gpl]: http://www.gnu.org/licenses/gpl-3.0.en.html
