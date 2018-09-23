/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "EffectHal_test"

#include <gtest/gtest.h>
#include <unistd.h>

int testVar1;
int testVar2;
char testFileName[128];

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    int opt;
    testVar1 = 5;
    testVar2 = 5;
    testFileName[0] = '\0';
    while((opt = getopt(argc, argv, "ntf:")) != -1) {
        switch(opt) {
        case 'n':
            testVar1 = 1;
            break;
        case 't':
            testVar2 = atoi(optarg);
            break;
        case 'f':
            if(strlen(optarg)<128)
                strncpy(testFileName, optarg, strlen(optarg));
            break;
        }
    }
    
    return RUN_ALL_TESTS();
}

