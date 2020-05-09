//=--GatherTypes.h------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Types used in GatherTool
//===----------------------------------------------------------------------===//

#ifndef __GATHERTYPES_H_
#define __GATHERTYPES_H_

typedef enum { CHECKED, WILD } IsChecked;

typedef std::map<std::string, std::vector<IsChecked>> ParameterMap;

#endif
