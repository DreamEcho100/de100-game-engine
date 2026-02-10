#!/bin/bash

# sudo apt install inotify-tools
# while inotifywait -r -e modify,create,delete \
# --exclude '(build|\.git)' \
# src .; do
#     ./build-dev.sh --build-game
# done

# sudo apt install entr
find src include -type f \
! -path "." \
| entr -c ./build-dev.sh --build-game

# pip install watchdog
# from watchdog.observers import Observer
# from watchdog.events import FileSystemEventHandler
# import subprocess, time

# class RebuildHandler(FileSystemEventHandler):
#     def on_any_event(self, event):
#         if "build" in event.src_path:
#             return
#         subprocess.run(["./build.sh", "arg1", "arg2"])

# observer = Observer()
# observer.schedule(RebuildHandler(), path=".", recursive=True)
# observer.start()

# try:
#     while True:
#         time.sleep(1)
# except KeyboardInterrupt:
#     observer.stop()

# observer.join()
