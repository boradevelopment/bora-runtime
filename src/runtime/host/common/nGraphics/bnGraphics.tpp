#pragma once
#include "nWindow/bnWindow.h"
#include "bnGraphics.h"

template<typename T>
void bnGraphics::DestroyResource(T*& buffer)
{
    commands.push_back(std::make_unique<DestroyResourceCommand>((IResourceHandle**)&buffer, &relVariables));
}

