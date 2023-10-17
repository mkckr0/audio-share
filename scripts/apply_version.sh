pushd $(dirname $0)/.. &>/dev/null

version_name=$(bash ./scripts/get_version.sh -n)
version_code=$(bash ./scripts/get_version.sh -c)
file_version0=$version_name.0
file_version1=$(echo $file_version0 | sed -En 's|\.|,|gp')
product_version=$file_version1

echo VERSION: \
    version_name=$version_name \
    version_code=$version_code

sed -Ebi "s|versionName\s*=\s*\"[^\"]*\"|versionName = \"$version_name\"|g" android-app/app/build.gradle.kts
sed -Ebi "s|versionCode\s*=\s*[0-9]*|versionCode = $version_code|g" android-app/app/build.gradle.kts

sed -Ebi "s|\tVERSION\s+[0-9.]*|\tVERSION $version_name|g" server-core/CMakeLists.txt

sed -Ebi "s|Audio Share Server, Version [^\"]*|Audio Share Server, Version $version_name|g" server-mfc/AudioShareServer.rc
sed -Ebi "s|FILEVERSION [0-9,]*|FILEVERSION $file_version1|g" server-mfc/AudioShareServer.rc
sed -Ebi "s|PRODUCTVERSION [0-9,]*|PRODUCTVERSION $product_version|g" server-mfc/AudioShareServer.rc
sed -Ebi "s|\"FileVersion\", \"[^\"]*\"|\"FileVersion\", \"$version_name.0\"|g" server-mfc/AudioShareServer.rc
sed -Ebi "s|\"ProductVersion\", \"[^\"]*\"|\"ProductVersion\", \"$version_name\"|g" server-mfc/AudioShareServer.rc

popd &>/dev/null