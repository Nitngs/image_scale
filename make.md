./configure --prefix=/home/nitangshuai/ffmpeg/build --libdir=/home/nitangshuai/ffmpeg/build/lib --enable-cross-compile --arch=mips --target-os=linux --cross-prefix=/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu- --disable-gpl --enable-nonfree --enable-error-resilience --disable-debug --enable-small --enable-shared --disable-ffmpeg --disable-ffprobe --disable-ffplay --disable-programs --disable-symver --disable-doc --disable-network --disable-htmlpages --disable-manpages --disable-podpages --disable-txtpages --enable-avformat --enable-avcodec --disable-avfilter --disable-avdevice --disable-postproc --disable-swscale --disable-swresample --disable-yasm --disable-asm --disable-autodetect --disable-server --disable-everything --enable-decoder=h264 --enable-decoder=hevc --enable-parser=h264 --enable-parser=hevc --enable-parser=jpeg2000 --enable-decoder=jpeg20002018192

./configure --prefix=/home/nitangshuai/ffmpeg/build --libdir=/home/nitangshuai/ffmpeg/build/lib --enable-cross-compile --arch=mips --target-os=linux --cross-prefix=/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu- 

g++ -g test.cpp -I/home/nitangshuai/opencv-4.x/build/install/include/opencv4 -L/home/nitangshuai/opencv-4.x/build/install/lib -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_highgui -o test
export LD_LIBRARY_PATH=/home/nitangshuai/opencv-4.x/build/install/lib:$LD_LIBRARY_PATH

g++ -g cali.cpp -I/home/nitangshuai/opencv-4.x/build/install/include/opencv4 -L/home/nitangshuai/opencv-4.x/build/install/lib -lopencv_core -lopencv_imgcodecs -lopencv_imgproc -lopencv_calib3d -o test

g++ -g test_jpeg.cpp -I/home/nitangshuai/libjpeg-turbo-main/build/libjpeg/include -L/home/nitangshuai/libjpeg-turbo-main/build/libjpeg/lib -ljpeg -o test
export LD_LIBRARY_PATH=/home/nitangshuai/libjpeg-turbo-main/build/libjpeg/lib:$LD_LIBRARY_PATH

g++ -g -c test_jpeg.cpp oil_resample.c -I/home/nitangshuai/libjpeg-turbo-main/build/libjpeg/include
g++ test_jpeg.o oil_resample.o -L/home/nitangshuai/libjpeg-turbo-main/build/libjpeg/lib -ljpeg -o test

export CC="/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu-gcc"
export CC="/opt/toolchains/sigmastar/arm-buildroot-linux-uclibcgnueabihf-4.9.4-uclibc-1.0.31/bin/arm-linux-gcc"
cmake -G"Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/home/nitangshuai/libjpeg-turbo-main/build3/ -DCMAKE_C_FLAGS=-muclibc ..

devcmd devinfo

 -lopencv_aruco -lopencv_barcode -lopencv_bgsegm -lopencv_bioinspired -lopencv_calib3d -lopencv_ccalib -lopencv_core -lopencv_datasets -lopencv_dnn_objdetect -lopencv_dnn -lopencv_dnn_superres -lopencv_dpm -lopencv_face -lopencv_features2d -lopencv_flann -lopencv_freetype -lopencv_fuzzy -lopencv_gapi -lopencv_hfs -lopencv_highgui -lopencv_imgcodecs -lopencv_img_hash -lopencv_imgproc -lopencv_intensity_transform -lopencv_line_descriptor -lopencv_mcc -lopencv_ml -lopencv_objdetect -lopencv_optflow -lopencv_phase_unwrapping -lopencv_photo -lopencv_plot -lopencv_quality -lopencv_rapid -lopencv_reg -lopencv_rgbd -lopencv_saliency -lopencv_shape -lopencv_stereo -lopencv_stitching -lopencv_structured_light -lopencv_superres -lopencv_surface_matching -lopencv_text -lopencv_tracking -lopencv_videoio -lopencv_video -lopencv_videostab -lopencv_wechat_qrcode -lopencv_xfeatures2d -lopencv_ximgproc -lopencv_xobjdetect -lopencv_xphoto

cmake ../ \
-DCMAKE_C_COMPILER=/opt/toolchains/sigmastar/arm-buildroot-linux-uclibcgnueabihf-4.9.4-uclibc-1.0.31/bin/arm-linux-gcc \
-DCMAKE_CXX_COMPILER=/opt/toolchains/sigmastar/arm-buildroot-linux-uclibcgnueabihf-4.9.4-uclibc-1.0.31/bin/arm-linux-g++ \
-DOPENCV_FORCE_3RDPARTY_BUILD=ON \
-DBUILD_ZLIB=ON -DWITH_GTK=OFF -DWITH_GTK=OFF \
-DWITH_GTK_2_X=OFF -DWITH_CUDA=OFF -DWITH_IPP=OFF \
-DWITH_OPENCL=OFF -DWITH_OPENCLAMDBLAS=OFF \
-DWITH_QUIRC=OFF -DWITH_OPENCLAMDFFT=OFF \
-DWITH_1394=OFF -DWITH_FFMPEG=OFF -DWITH_WEBP=OFF \
-DWITH_TIFF=OFF -DWITH_OPENEXR=OFF -DWITH_PNG=OFF \
-DWITH_PROTOBUF=OFF -DWITH_GSTREAMER=OFF -DWITH_IMGCODEC_SUNRASTER=OFF \
-DBUILD_SHARED_LIBS=ON -DBUILD_opencv_ts=OFF \
-DBUILD_opencv_shape=OFF -DBUILD_opencv_stitching=OFF \
-DBUILD_opencv_apps=OFF -DBUILD_opencv_calib3d=OFF \
-DBUILD_opencv_dnn=OFF -DBUILD_opencv_features2d=ON \
-DBUILD_opencv_flann=OFF -DBUILD_opencv_highgui=OFF \
-DBUILD_opencv_ml=OFF -DBUILD_opencv_objdetect=OFF \
-DBUILD_opencv_photo=OFF -DBUILD_opencv_video=OFF \
-DBUILD_opencv_videoio=OFF -DBUILD_opencv_videostab=OFF \
-DCMAKE_BUILD_TYPE=RELEASE \
-DCMAKE_INSTALL_PREFIX=/home/nitangshuai/opencv-4.x/build2 \
-DCMAKE_CXX_FLAGS="-s -Os -latomic" -DCMAKE_C_FLAGS="-s -Os -latomic"

cmake ../ \
-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
-DOPENCV_FORCE_3RDPARTY_BUILD=ON \
-DBUILD_ZLIB=ON -DWITH_GTK=OFF \
-DWITH_GTK_2_X=OFF -DWITH_CUDA=OFF -DWITH_IPP=OFF \
-DWITH_OPENCL=OFF -DWITH_OPENCLAMDBLAS=OFF \
-DWITH_QUIRC=OFF -DWITH_OPENCLAMDFFT=OFF \
-DWITH_1394=OFF -DWITH_FFMPEG=OFF -DWITH_WEBP=OFF \
-DWITH_TIFF=OFF -DWITH_OPENEXR=OFF -DWITH_PNG=OFF \
-DWITH_PROTOBUF=OFF -DWITH_GSTREAMER=OFF -DWITH_IMGCODEC_SUNRASTER=OFF \
-DBUILD_SHARED_LIBS=ON -DBUILD_opencv_ts=OFF \
-DBUILD_opencv_shape=OFF -DBUILD_opencv_stitching=ON \
-DBUILD_opencv_apps=OFF -DBUILD_opencv_calib3d=ON \
-DBUILD_opencv_dnn=OFF -DBUILD_opencv_features2d=ON \
-DBUILD_opencv_flann=ON -DBUILD_opencv_highgui=OFF \
-DBUILD_opencv_ml=OFF -DBUILD_opencv_objdetect=OFF \
-DBUILD_opencv_photo=OFF -DBUILD_opencv_video=ON \
-DBUILD_opencv_videoio=ON -DBUILD_opencv_videostab=OFF \
-DCMAKE_BUILD_TYPE=RELEASE \
-DCMAKE_INSTALL_PREFIX=/home/nitangshuai/opencv-4.7.0/build/install \
-DOPENCV_EXTRA_MODULES_PATH=/home/nitangshuai/opencv_contrib-4.7.0/modules \
-DCMAKE_CXX_FLAGS="-s -Os -latomic" -DCMAKE_C_FLAGS="-s -Os -latomic"

/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu-g++ -O3 -c test_low_mem.cpp scale.c cpu_id.c -I/home/nitangshuai/libjpeg-turbo-main/build/libjpeg/include -muclibc
/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu-g++ test_low_mem.o scale.o cpu_id.o -L/home/nitangshuai/libjpeg-turbo-main/build2/lib -lturbojpeg -muclibc -o test

/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu-g++ test_low_mem.cpp -I ~/libjpeg-turbo-main/build/libjpeg/include/ -L./ -ljpeg -muclibc -o test
g++ test_low_mem.cpp -I ~/libjpeg-turbo-main/build/libjpeg/include/ -L ~/libjpeg-turbo-main/build/libjpeg/lib/ -ljpeg -o test

/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu-g++ test_low_mem.cpp scale.c cpu_id.c -I ~/libjpeg-turbo-main/build/libjpeg/include/ -muclibc
/opt/toolchains/ingenic/t41/mips-gcc720-glibc229-r5.1.4/bin/mips-linux-gnu-g++ test_low_mem.cpp scale.c cpu_id.c -I ~/libjpeg-turbo-main/build/libjpeg/include/ -muclibc -L ~/libjpeg-turbo-main/build2/lib/ -lturbojpeg

export CC=/opt/toolchains/ingenic/t40/mips-linux-gnu-ingenic-gcc7.2.0-glibc2.29-fp64/bin/mips-linux-gnu-gcc
export CXX=/opt/toolchains/ingenic/t40/mips-linux-gnu-ingenic-gcc7.2.0-glibc2.29-fp64/bin/mips-linux-gnu-g++
export PATH=/opt/toolchains/ingenic/t40/mips-linux-gnu-ingenic-gcc7.2.0-glibc2.29-fp64/bin:$PATH
./configure --prefix=/home/nitangshuai/opus-1.3.1/build/ --host=mips-linux-gnu --enable-fixed-point --disable-float-api CFLAGS="-Os -muclibc"

/opt/toolchains/arm-anykav500-linux-uclibcgnueabi/bin/arm-anykav500-linux-uclibcgnueabi-gcc test_win.cpp -mfloat-abi=softfp -mfpu=neon -o test
