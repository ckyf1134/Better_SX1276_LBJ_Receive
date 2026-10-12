#include <stdio.h>
#include <string.h>
struct Train {
    char checi[6];
    char startstation[50];
    char endstation[50];
};
struct Train KeChe[3] = {
    {"G101", "北京南", "上海虹桥"},
    {"D31", "北京", "哈尔滨"},
    {"Z2", "上海", "北京"}
};