# image-processing-system
This is a distributed image processing system built in C++, capable of generating, processing and storing data.

## Structure
- `include/` — shared headers accessible by all executables  
- `image_generator/`, `feature_extractor/`, `data_logger/` modules — each has its own:
  - `include/` (private headers)
  - `src/` (source files)
  - `main.cpp`
  - `Makefile`

# How to Install and Run 
below is a detailed install/build guide, simply execute the bash commands in each code block below in sequence. 

## 1) clone repo
First, open a new terminal window and run:
```bash
git clone https://github.com/TimurKhayrullin/image-processing-system.git
cd image-processing-system
```

## 2) Install Dependencies
 
### Linux (Tested with Ubuntu 24.04.3 LTS)

#### Basic compilation tools
```bash
sudo apt update && sudo apt install -y make cmake g++ wget unzip 
```

for libraries that we build from source, so it's advised to make a folder to contain all dependencies' source files:
```bash
cd ..
mkdir ips-dependencies
```

#### OpenCV 
We install OpenCV from source, guide is taken from https://docs.opencv.org/4.x/d7/d9f/tutorial_linux_install.html
```bash
cd ips-dependencies
wget -O opencv.zip https://github.com/opencv/opencv/archive/4.x.zip
mv opencv-4.x opencv
cd opencv
mkdir -p build && cd build
cmake ../
make -j4 # After successful build you will find libraries in the build/lib directory and executables (test, samples, apps) in the build/bin directory
sudo make install
opencv_version # should return "4.13.0-dev"
```

#### Yaml-cpp
```bash
sudo apt-get install libyaml-cpp-dev
```

#### ZeroMQ 
```bash
sudo apt-get install libzmq3-dev
sudo apt-get install cppzmq-dev
```

#### Postgresql
```bash
sudo apt-get install libpq-dev
sudo apt-get install libpqxx-dev
sudo apt-get install postgresql postgresql-contrib
```

### MacOS:
For MacOS we used the brew package manager: https://brew.sh/ 
```bash
brew install zmq 
brew install cppzmq
brew install libpqxx 
brew install yaml-cpp     
brew install libpq 
brew install cmake
brew install opencv
```

## 3) Start and configure database

# Linux (Tested with Ubuntu 24.04.3 LTS)
First, start the postgres service:
```bash
sudo systemctl start postgresql # start postgres service
sudo -u postgres createuser -s postgres # create superuser role
sudo -u postgres psql -c "ALTER USER postgres PASSWORD 'mypass';" # set superuser password to 'mypass'
createdb -h 127.0.0.1 -U postgres telemetry # create database called "telemetry"
sudo -u postgres psql -lqt | grep -w telemetry # check that the database was created, should output something like "telemetry | postgres | UTF8 ..."
```

# MacOS (requires Docker Desktop):
```bash
brew services start postgresql@16
docker run --name pg -e POSTGRES_PASSWORD=mypass -e POSTGRES_DB=telemetry -p 5432:5432 -d postgres:16
createuser -s postgres             // if the role doesn't exist
psql -h 127.0.0.1 -U postgres -c "ALTER USER postgres PASSWORD 'mypass';"
createdb -h 127.0.0.1 -U postgres telemetry
psql -h 127.0.0.1 -U postgres -p 5432 -W
```


## 4) Build image-processing-system binary
To build all 3 executables (image_generator, feature_extractor, data_logger) at once, run the following bash from a terminal set to the main project directory:
```bash
make 
```

To build the individual executables run the following
```bash
cd [module] && make
```
For example:
```bash
cd image_generator && make
```


## Start Database server
# MacOS:
```bash
brew services start postgresql@16
docker run --name pg -e POSTGRES_PASSWORD=mypass -e POSTGRES_DB=telemetry -p 5432:5432 -d postgres:16
createuser -s postgres             // if the role doesn't exist
psql -h 127.0.0.1 -U postgres -c "ALTER USER postgres PASSWORD 'mypass';"
createdb -h 127.0.0.1 -U postgres telemetry
psql -h 127.0.0.1 -U postgres -p 5432 -W
```

## 5) Run applications
To run the applications, invoke the respective executable found in the build folder. here are examples for invoking all 3 from the top-level project directory
```bash
./build/image_generator/image_generator
./build/feature_extractor/feature_extractor
./build/data_logger/data_logger
```

## Some useful commands for testing/sanity checks

### postgres cli for prompting
psql -U postgres -d telemetry

### delete database
psql -U postgres -c "DROP DATABASE telemetry;"

### clear the data in the database but keep defined tables and columns
psql -U postgres -d telemetry -c "TRUNCATE TABLE payloads RESTART IDENTITY;"


## Links to d used for testing
### small 
https://li-chongyi.github.io/proj_benchmark.html 
https://github.com/dlut-dimt/Realworld-Underwater-Image-Enhancement-RUIE-Benchmark/blob/master/UIQS.rar 

### medium
http://data.vision.ee.ethz.ch/cvl/DIV2K/DIV2K_valid_HR.zip

### large
https://zenodo.org/records/5744037 

