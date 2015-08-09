/*
    RenderWare driver virtualization framework.
    Drivers are a reconception of the original pipeline design of Criterion.
    A driver has its own copies of instanced geometry, textures and materials.
    Those objects have implicit references to instanced handles.

    If an atomic is asked to be rendered, it is automatically instanced.
    Instancing can be sheduled asynchronically way before the rendering to improve performance.
*/

struct Driver
{
    // TODO.
};