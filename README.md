# image-processing-system
This is a distributed image processing system built in C++, capable of generating, processing and storing data.

## Structure
- `include/` — shared headers accessible by all executables  
- `image_generator/`, `feature_extractor/`, `data_logger/` modules — each has its own:
  - `include/` (private headers)
  - `src/` (source files)
  - `main.cpp`
  - `Makefile`


### install dependencies
## MacOS:
# barebones functionality
brew install zmq
brew install cppzmq
brew install libpqxx 
brew install yaml-cpp     
brew install libpq 

# openCV
brew install cmake
brew install opencv
 
# Ubuntu
sudo apt install libpqxx-dev  # Ubuntu/Debian


## Build individual modules
cd [module] && make
ex.: cd image_generator && make

## Build Everything
// ```bash
make

## Start Database server
# MacOS:
brew services start postgresql@16
docker run --name pg -e POSTGRES_PASSWORD=mypass -e POSTGRES_DB=telemetry -p 5432:5432 -d postgres:16
createuser -s postgres             // if the role doesn't exist
psql -h 127.0.0.1 -U postgres -c "ALTER USER postgres PASSWORD 'mypass';"
createdb -h 127.0.0.1 -U postgres telemetry
psql -h 127.0.0.1 -U postgres -p 5432 -W

## Run modules
./build/image_generator/image_generator
./build/feature_extractor/feature_extractor
./build/data_logger/data_logger

## postgres cli for prompting
psql -U postgres -d telemetry

## delete database
psql -U postgres -c "DROP DATABASE telemetry;"

## clear the data in the database but keep defined tables and columns
psql -U postgres -d telemetry -c "TRUNCATE TABLE payloads RESTART IDENTITY;"


## some links to datasets
# small 
https://li-chongyi.github.io/proj_benchmark.html 
https://github.com/dlut-dimt/Realworld-Underwater-Image-Enhancement-RUIE-Benchmark/blob/master/UIQS.rar 

# medium
http://data.vision.ee.ethz.ch/cvl/DIV2K/DIV2K_valid_HR.zip

# large
https://zenodo.org/records/5744037 

