root_dir=$(realpath .)
platform=$1
pushd out/install &>/dev/null

rm -rf audio-share-server-cmd
cp -r ${platform}-Release audio-share-server-cmd
cp ${root_dir}/../LICENSE audio-share-server-cmd

if [ ${platform} = "linux" ]
then
    filename=audio-share-server-cmd-linux.tar.gz
    tar -czvf ${filename} audio-share-server-cmd
elif [ ${platform} = "windows" ]
then
    filename=audio-share-server-cmd-windows.zip
    7z a ${filename} audio-share-server-cmd
fi
sha256sum ${filename} | tee ${filename}.sha256

popd &>/dev/null