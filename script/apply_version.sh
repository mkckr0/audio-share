pushd $(dirname $0)/.. &>/dev/null

version_name=$(./script/get_version.sh -n)
version_code=$(./script/get_version.sh -c)
file_version0=$version_name.0
file_version1=$(echo $file_version0 | sed -En 's|\.|,|gp')
product_version=$file_version1

echo VERSION: \
    version_name=$version_name \
    version_code=$version_code

sed -Ebi "s|versionName\s*=\s*\"[^\"]*\"|versionName = \"$version_name\"|g" audio-share-app/app/build.gradle.kts
sed -Ebi "s|versionCode\s*=\s*[0-9]*|versionCode = $version_code|g" audio-share-app/app/build.gradle.kts

sed -Ebi "s|Audio Share Server, Version [^\"]*|Audio Share Server, Version $version_name|g" audio-share-server/AudioShareServer.rc
sed -Ebi "s|FILEVERSION [0-9,]*|FILEVERSION $file_version1|g" audio-share-server/AudioShareServer.rc
sed -Ebi "s|PRODUCTVERSION [0-9,]*|PRODUCTVERSION $product_version|g" audio-share-server/AudioShareServer.rc
sed -Ebi "s|\"FileVersion\", \"[^\"]*\"|\"FileVersion\", \"$version_name.0\"|g" audio-share-server/AudioShareServer.rc
sed -Ebi "s|\"ProductVersion\", \"[^\"]*\"|\"ProductVersion\", \"$version_name\"|g" audio-share-server/AudioShareServer.rc

popd &>/dev/null