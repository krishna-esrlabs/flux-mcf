# Building and Running MCF with Docker

## Dependencies
* [Docker](https://docs.docker.com/get-docker/)
* (Optional) [Nvidia Container Toolkit (nvidia-docker)](https://github.com/NVIDIA/nvidia-docker) - Needed to build MCF with CUDA support.

## Building Docker Image
* The Docker image should be built once initially and then re-built every time the Dockerfile is modified.
* *(Option 1) Without CUDA support)*:

      docker build -t <docker_image_name> -f docker/without_cuda/Dockerfile .

* *(Option 2) With CUDA support*:
    
      docker build -t <cuda_docker_image_name> -f docker/with_cuda/Dockerfile .

```{note}
<docker_image_name> and <cuda_docker_image_name> are the names of the generated docker images, which can be named 
anything. The same name will be used in the `run` command below. 
```
  
## Running Docker Container

* Run the docker container in an interactive bash terminal.

    * *(Option 1) Without CUDA support)*:
        
          docker run -it --ipc="host" --user $(id -u):$(id -g) -v /path/to/mcf:/home/mcf -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro <docker_image_name> bash

    * *(Option 2) With CUDA support*:
 
          nvidia-docker run -it --ipc="host" --user $(id -u):$(id -g) -v /path/to/mcf:/home/mcf -v /etc/passwd:/etc/passwd:ro -v /etc/group:/etc/group:ro <cuda_docker_image_name> bash
        
    ```{note}
    The `"-v /path/to/mcf:/home/mcf"` command will mount the MCF repository within the docker container. So all 
    changes made to `/home/mcf/*` inside the docker container will be reflected in `/path/to/mcf/*` outside the docker
    container.
    ```
        

* Follow the instructions in [GETTING_STARTED.md](../GETTING_STARTED.md#building-mcf) to build MCF within the docker container.