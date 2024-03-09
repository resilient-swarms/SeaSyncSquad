# SeaSyncSquad

## Build instruction
``` 
# Install conda and conda-forge
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh
bash ~/miniconda3/miniconda.sh
rm -rf ~/miniconda3/miniconda.sh
conda update conda
conda config --add channels conda-forge
conda config --set channel_priority strict

# Install dependencies using conda
conda create --name seasyncsquad
conda activate seasyncsquad
conda install gcc gxx_linux-64 cmake python numpy matplotlib seaborn fmt

# Clone repository and compile
git clone --recurse-submodules https://github.com/resilient-swarms/SeaSyncSquad.git
cd seasyncsquad
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make 
```
