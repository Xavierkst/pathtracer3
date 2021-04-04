#include "SceneLoader.h"

void SceneLoader::rightMultiply(const optix::Matrix4x4& M)
{
    optix::Matrix4x4& T = transStack.top();
    T = T * M;
}

optix::float3 SceneLoader::transformPoint(optix::float3 v)
{
    optix::float4 vh = transStack.top() * optix::make_float4(v, 1);
    return optix::make_float3(vh) / vh.w; 
}

optix::float3 SceneLoader::transformNormal(optix::float3 n)
{
    return optix::make_float3(transStack.top() * make_float4(n, 0));
}

template <class T>
bool SceneLoader::readValues(std::stringstream& s, const int numvals, T* values)
{
    for (int i = 0; i < numvals; i++)
    {
        s >> values[i];
        if (s.fail())
        {
            std::cout << "Failed reading value " << i << " will skip" << std::endl;
            return false;
        }
    }
    return true;
}


std::shared_ptr<Scene> SceneLoader::load(std::string sceneFilename)
{
    // Attempt to open the scene file 
    std::ifstream in(sceneFilename);
    if (!in.is_open())
    {
        // Unable to open the file. Check if the filename is correct.
        throw std::runtime_error("Unable to open scene file " + sceneFilename);
    }

    auto scene = std::make_shared<Scene>();

    transStack.push(optix::Matrix4x4::identity());

    std::string str, cmd;

    // save material values to assign to each geometry
    optix::float3 diffuse = optix::make_float3(.0f);
    optix::float3 ambient = optix::make_float3(0.2f);
    optix::float3 specular = optix::make_float3(.0f);
    optix::float3 emission = optix::make_float3(.0f);
    float shininess = .0f;

    // default attenuation values to save:
    Attenuation atten = Attenuation();

    // Read a line in the scene file in each iteration
    while (std::getline(in, str))
    {
        // Ruled out comment and blank lines
        if ((str.find_first_not_of(" \t\r\n") == std::string::npos) 
            || (str[0] == '#'))
        {
            continue;
        }

        // Read a command
        std::stringstream s(str);
        s >> cmd;

        // Some arrays for storing values
        float fvalues[12];
        int ivalues[3];
        std::string svalues[1];

        if (cmd == "size" && readValues(s, 2, fvalues))
        {
            scene->width = (unsigned int)fvalues[0];
            scene->height = (unsigned int)fvalues[1];
        }
        else if (cmd == "output" && readValues(s, 1, svalues))
        {
            scene->outputFilename = svalues[0];
        }
        // TODO: use the examples above to handle other commands

        else if ( cmd == "maxdepth" && readValues(s, 1, fvalues)) {
            scene->depth = (unsigned int)fvalues[0]; 
        } 

        else if ( cmd == "camera" && readValues(s, 10, fvalues)) {
            scene->camera = Camera(optix::make_float3(fvalues[0], fvalues[1], fvalues[2]), 
                    optix::make_float3(fvalues[3], fvalues[4], fvalues[5]), 
                    optix::make_float3(fvalues[6], fvalues[7], fvalues[8]),
                    fvalues[9]);  
        } 
        
        else if (cmd == "diffuse" && readValues(s, 3, fvalues)) {
            diffuse = optix::make_float3(fvalues[0], fvalues[1], fvalues[2]);
        }

        else if (cmd == "ambient" && readValues(s, 3, fvalues)) {
            ambient = optix::make_float3(fvalues[0], fvalues[1], fvalues[2]);
        }

        else if (cmd == "specular" && readValues(s, 3, fvalues)) {
            specular = optix::make_float3(fvalues[0], fvalues[1], fvalues[2]);
        }

        else if (cmd == "emission" && readValues(s, 3, fvalues)) {
            emission = optix::make_float3(fvalues[0], fvalues[1], fvalues[2]);
        }

        else if (cmd == "shininess" && readValues(s, 1, fvalues)) {
            shininess = fvalues[0];
        }

        else if (cmd == "attenuation" && readValues(s, 3, fvalues)) {
            atten.constant = fvalues[0];
            atten.linear = fvalues[1];
            atten.quadratic = fvalues[2];
        }

        else if (cmd == "directional" && readValues(s, 6, fvalues)) {
           scene->dlights.push_back(DirectionalLight(
               optix::make_float3(fvalues[0], fvalues[1], fvalues[2]), 
               optix::make_float3(fvalues[3], fvalues[4], fvalues[5]), 
               atten));
        }

        else if (cmd == "point" && readValues(s, 6, fvalues)) {
           scene->plights.push_back(PointLight(
               optix::make_float3(fvalues[0], fvalues[1], fvalues[2]), 
               optix::make_float3(fvalues[3], fvalues[4], fvalues[5]), 
               atten));
        }

        else if (cmd == "maxverts" && readValues(s, 1, fvalues)) {
           scene->maxverts = (unsigned int)fvalues[0]; 
        }
        
        else if (cmd == "maxvertnorms" && readValues(s, 1, fvalues)) {
           scene->maxvertnorms = (unsigned int)fvalues[0]; 
        }

        // next is Geometry, and Transformations
        else if (cmd == "sphere" && readValues(s, 4, fvalues)) {
           scene->spheres.push_back(
               Sphere(optix::make_float3(
                   fvalues[0], fvalues[1], fvalues[2]), fvalues[3], 
                   ambient, shininess, diffuse, specular, emission));
        }

        else if (cmd == "vertex" && readValues(s, 3, fvalues)) {
            scene->vertices.push_back(
                optix::make_float3(fvalues[0], fvalues[1], fvalues[2]));
        }

        else if (cmd == "vertexnormal" && readValues(s, 6, fvalues)) {
            // scene->vertexNormals.push_back(std::make_pair(
            //     optix::make_float3(fvalues[0], fvalues[1], fvalues[2]), 
            //     optix::make_float3(fvalues[3], fvalues[4], fvalues[5])));
            scene->vertices.push_back(
                optix::make_float3(fvalues[0], fvalues[1], fvalues[2]));
            scene->normals.push_back(
                optix::make_float3(fvalues[3], fvalues[4], fvalues[5]));
        }

        else if (cmd == "tri" && readValues(s, 3, ivalues)) {
            scene->triangles.push_back(Triangle(
                scene->vertices[ivalues[0]], 
                scene->vertices[ivalues[1]], 
                scene->vertices[ivalues[2]], 
                ambient, shininess, diffuse, 
                specular, emission)); 
        }

         else if (cmd == "trinormal" && readValues(s, 3, ivalues)) {
            scene->triangles.push_back(Triangle(
                // load in vertices
                scene->vertices[ivalues[0]], 
                scene->vertices[ivalues[1]], 
                scene->vertices[ivalues[2]], 
                // load in normals corresp to vertices
                scene->normals[ivalues[0]], 
                scene->normals[ivalues[1]], 
                scene->normals[ivalues[2]], 
                ambient, shininess, diffuse, 
                specular, emission)); 
        }

       
    }

    in.close();

    return scene;
}