#!/usr/bin/env python3
"""
make_video_from_images.py

Create an MPEG video from a list of PNG/JPEG image files.

The total video duration is provided by the user. The script computes how long
each image should be shown so that the final video has approximately that
duration.

The image files are used in the same order as listed on the command line,
unless --reverse is used.

Examples:
    python3 make_video_from_images.py -t 10 -o movie.mpg frame001.jpeg frame002.jpeg frame003.jpeg

    python3 make_video_from_images.py -t 10 -o movie.mpg --reverse frame001.jpeg frame002.jpeg frame003.jpeg

Requirements:
    ffmpeg must be available in your PATH.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


SUPPORTED_EXTENSIONS = {".png", ".jpg", ".jpeg"}


def check_input_files(files):
    """
    Check that all input files exist and have supported extensions.

    The order of the returned list is the same as the order provided
    on the command line.
    """

    if not files:
        raise ValueError("No input image files were provided.")

    checked_files = []

    for file_name in files:
        path = Path(file_name)

        if not path.exists():
            raise FileNotFoundError(f"Input file does not exist: {path}")

        if not path.is_file():
            raise ValueError(f"Input path is not a regular file: {path}")

        if path.suffix.lower() not in SUPPORTED_EXTENSIONS:
            raise ValueError(
                f"Unsupported file extension for {path}. "
                "Supported extensions are: .png, .jpg, .jpeg"
            )

        checked_files.append(path)

    return checked_files


def write_ffmpeg_concat_file(image_files, frame_duration, concat_file):
    """
    Write an ffmpeg concat-demuxer input file.

    Each image is assigned the same duration. The last image is written twice,
    which is required by ffmpeg so the final frame duration is respected.
    """

    with open(concat_file, "w", encoding="utf-8") as f:
        for image_file in image_files:
            abs_path = image_file.resolve()
            f.write(f"file '{abs_path}'\n")
            f.write(f"duration {frame_duration:.10f}\n")

        # Repeat the last file so ffmpeg keeps it on screen for its duration.
        f.write(f"file '{image_files[-1].resolve()}'\n")


def make_video(image_files, output_file, total_duration, ffmpeg_cmd, overwrite):
    """
    Create the video using ffmpeg.
    """

    n_frames = len(image_files)
    frame_duration = total_duration / n_frames

    with tempfile.NamedTemporaryFile(
        mode="w",
        suffix=".txt",
        prefix="ffmpeg_image_list_",
        delete=False,
    ) as temp:
        concat_file = Path(temp.name)

    try:
        write_ffmpeg_concat_file(
            image_files=image_files,
            frame_duration=frame_duration,
            concat_file=concat_file,
        )

        cmd = [ffmpeg_cmd]

        if overwrite:
            cmd.append("-y")
        else:
            cmd.append("-n")

        cmd += [
            "-f",
            "concat",
            "-safe",
            "0",
            "-i",
            str(concat_file),
            "-vf",
            "scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p",
            "-c:v",
            "mpeg2video",
            "-q:v",
            "2",
            str(output_file),
        ]

        print("Running command:")
        print(" ".join(str(x) for x in cmd))

        subprocess.run(cmd, check=True)

    finally:
        if concat_file.exists():
            concat_file.unlink()


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Assemble an MPEG video from PNG/JPEG files. "
            "The total video duration is given with -t/--time, and the image "
            "files are provided afterward in the order they should appear. "
            "Use --reverse to assemble them in reverse order."
        )
    )

    parser.add_argument(
        "-t",
        "--time",
        type=float,
        required=True,
        help="Total video duration in seconds.",
    )

    parser.add_argument(
        "-o",
        "--output",
        default="out.mpg",
        help="Output video file name. Default: out.mpg",
    )

    parser.add_argument(
        "--ffmpeg",
        default="ffmpeg",
        help="ffmpeg executable name or full path. Default: ffmpeg",
    )

    parser.add_argument(
        "-r",
        "--reverse",
        action="store_true",
        help="Assemble the input image files in reversed order.",
    )

    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite the output file if it already exists.",
    )

    parser.add_argument(
        "images",
        nargs="+",
        help="Input image files in the correct order.",
    )

    args = parser.parse_args()

    if args.time <= 0.0:
        print("ERROR: video time must be positive.", file=sys.stderr)
        sys.exit(1)

    output_file = Path(args.output)

    try:
        image_files = check_input_files(args.images)

        if args.reverse:
            image_files = list(reversed(image_files))

        print("Frame order:")
        for i, image_file in enumerate(image_files, start=1):
            print(f"  {i}: {image_file}")

        make_video(
            image_files=image_files,
            output_file=output_file,
            total_duration=args.time,
            ffmpeg_cmd=args.ffmpeg,
            overwrite=args.overwrite,
        )

    except Exception as err:
        print(f"ERROR: {err}", file=sys.stderr)
        sys.exit(1)

    print(f"Created video: {output_file}")


if __name__ == "__main__":
    main()
