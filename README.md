# DeTLoc Tracer: detect and trace communication between threads

Please see the following paper for more details:

M. Agung, M. A. Amrizal, R. Egawa and H. Takizawa, "DeLoc: A Locality and Memory-Congestion-Aware Task Mapping Method for Modern NUMA Systems," in IEEE Access, vol. 8, pp. 6937-6953, 2020, doi: 10.1109/ACCESS.2019.2963726.

## Usage

Prerequisite:
- Pin tool (tested with Pin 3.5, 3.6, and 3.7)

Compile:
    
    $ make

(If it fails detecting the Pin, please edit the Makefile)

To generate the communication matrix:

    $ ./run.sh -s_prod_simple -- ./your_program

If you find this work useful in your research, please cite the paper using this bibtex below:

```
    @ARTICLE{8949493,  
    author={M. {Agung} and M. A. {Amrizal} and R. {Egawa} and H. {Takizawa}},  
    journal={IEEE Access},   
    title={DeLoc: A Locality and Memory-Congestion-Aware Task Mapping Method for Modern NUMA Systems},  
    year={2020},  
    volume={8},  
    number={},  
    pages={6937-6953},  
    doi={10.1109/ACCESS.2019.2963726}}
```

