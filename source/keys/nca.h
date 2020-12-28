#pragma once

enum NcaTypes {
    Porgram = 0,
    Meta,
    Control,
    Manual,
    Data,
    PublicData
};

int GetNcaType(char *path);