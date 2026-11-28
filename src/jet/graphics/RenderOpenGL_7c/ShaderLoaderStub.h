// Fixed-function stand-in for the API module's Cg effect loader.
//
// The retail OpenGL display hands the game a CGShaderLoaderOpenGL (display slot 0x154) that really builds Cg
// techniques and constants. This backend runs GL 1.1 fixed function, so it returns the same interface with
// technique/constant objects that accept every value and apply nothing. The game then keeps its scenes alive
// instead of aborting when a SpeedTree (or similar) material asks for a shader set.
#pragma once

namespace Jet {
class CGShaderLoader;
}

// A loader owned by the caller (delete through CGShaderLoader's virtual destructor).
Jet::CGShaderLoader* __cdecl CreateShaderLoaderStub();
