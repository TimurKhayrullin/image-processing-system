# image-processing-system
This is a distributed image processing system built in C++, capable of generating, processing and storing data.

## 🧱 Structure
- `include/` — shared headers accessible by all executables  
- `image_generator/`, `feature_extractor/`, `data_logger/` modules — each has its own:
  - `include/` (private headers)
  - `src/` (source files)
  - `main.cpp`
  - `Makefile`


## install dependencies
MacOS:
brew install zmq
brew install cppzmq


## Build individual modules
cd [module] && make
ex.: cd image_generator && make

## Run modules
./build/image_generator/image_generator
./build/feature_extractor/feature_extractor
./build/data_logger/data_logger

## 🛠️ Build Everything
```bash
make
