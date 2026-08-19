#pragma once

#ifndef _CS2_VERSION
#define _CS2_VERSION latest
#endif

#ifndef _CS2_COMMIT
#define _CS2_COMMIT latest
#endif

#ifndef _CS2_OSVERSION
#define _CS2_OSVERSION unknown
#endif

#define STR1(m) #m
#define STR(m) STR1(m)

#define CS2_TITLE "ChuckStation2 (" STR(_CS2_VERSION) ")"
#define CS2_VULKAN_API_VERSION VK_API_VERSION_1_2