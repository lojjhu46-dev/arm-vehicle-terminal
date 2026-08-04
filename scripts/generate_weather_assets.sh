#裁剪图片的脚本
#!/usr/bin/env bash

set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_image="$project_root/resource/Weather/weather.png"
base_image="$project_root/resource/logo/mian_UI.jpeg"
output_dir="$project_root/resource/Weather"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/weather-assets.XXXXXX")

cleanup()
{
    rm -rf "$work_dir"
}
trap cleanup EXIT

if ! command -v convert >/dev/null 2>&1; then
    printf 'ImageMagick convert is required\n' >&2
    exit 1
fi

if [ ! -f "$source_image" ] || [ ! -f "$base_image" ]; then
    printf 'Missing weather source or main UI image\n' >&2
    exit 1
fi

mkdir -p "$output_dir"

# The source is a 4x3 atlas. Each crop leaves the Chinese label outside the
# working area, while leaving enough margin for the icon glow and snowflakes.
names=(
    sunny sunny_to_cloudy cloudy overcast
    light_rain moderate_rain heavy_rain thunderstorm
    light_snow heavy_snow fog haze
)
crops=(
    336x245+24+40 336x245+408+40 336x245+792+40 336x245+1176+40
    336x230+24+380 336x230+408+380 336x230+792+380 336x230+1176+380
    336x235+24+680 336x235+408+680 336x235+792+680 336x235+1176+680
)

convert "$base_image" -crop 96x64+352+68 +repage "$work_dir/base.jpeg"

for index in "${!names[@]}"; do
    name=${names[$index]}
    crop=${crops[$index]}
    subject="$work_dir/${name}.png"

    # Flood-fill the connected atlas background, retaining a soft alpha edge.
    # The intermediate PNG is temporary; the final asset is an opaque JPEG
    # containing the exact main-UI background at the runtime destination.
    convert "$source_image" -crop "$crop" +repage \
        -alpha on -fuzz 15% -fill none \
        -draw 'matte 0,0 floodfill' -trim +repage \
        -resize 72x54 "$subject"
    convert "$work_dir/base.jpeg" "$subject" -gravity center -composite \
        -quality 100 -sampling-factor 4:4:4 "$output_dir/${name}.jpeg"
done

printf 'Generated %d weather assets in %s\n' "${#names[@]}" "$output_dir"
