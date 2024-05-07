api_key=$1
version=$2

if [ -z $api_key ] || [ -z $version ]; then
    echo 'error arg'
    exit 1
fi

do_put() {
    api_key=$1
    default=$2
    project_name=$3
    file_path=$4
    echo setup $default $project_name \"$file_path\"
    curl -H "Accept: application/json" -X PUT -d "default=$default" -d "api_key=$api_key" "https://sourceforge.net/projects/$project_name/files/$file_path"
    echo -e '\n'
}

do_put $api_key windows audio-share "v$version/AudioShareServer.exe"
do_put $api_key linux audio-share "v$version/audio-share-server-cmd-linux.tar.gz"
do_put $api_key android audio-share "v$version/audio-share-app-$version-release.apk"
do_put $api_key others audio-share "v$version/v$version%20source%20code.tar.gz"
