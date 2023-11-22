#!/bin/bash
set -e

if [ ! -d `pwd`/build ]; then
  mkdir `pwd`/"build"
fi

rm -rf `pwd`/build/*

if [ -d usr/include/chat ]; then
  rm -rf usr/include/chat
fi

if [ -f usr/lib/libchat.so ]; then
  rm usr/lib/libchat.so
fi

cd build &&
   cmake .. &&
   make

cd ..

# 把头文件拷贝到 /usr/include/chat 下， so 库拷贝到 /usr/lib 下
if [ ! -d /usr/include/chat ]; then
  mkdir /usr/include/chat
fi

if [ ! -d /usr/include/chat/http ]; then
  mkdir /usr/include/chat/http
fi

if [ ! -d /usr/include/chat/streams ]; then
  mkdir /usr/include/chat/streams
fi

for header in `ls ./chat/*.h`
do
  cp $header /usr/include/chat
done

for header in `ls ./chat/http/*.h`
do
  cp $header /usr/include/chat/http
done

for header in `ls ./chat/streams/*.h`
do
  cp $header /usr/include/chat/streams
done

cp `pwd`/lib/libchat.so /usr/lib

ldconfig