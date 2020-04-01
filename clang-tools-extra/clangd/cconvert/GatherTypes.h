#ifndef __GATHERTYPES_H_
#define __GATHERTYPES_H_

typedef enum {
	CHECKED,
	WILD
} IsChecked;


typedef std::map<std::string, std::vector<IsChecked>> ParameterMap;


#endif
