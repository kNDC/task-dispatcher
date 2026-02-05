#pragma once

#include <cstdint>
#include <map>

namespace dispatcher
{
    enum class TaskPriority : char { High, Normal };
    const static size_t n_priorities = 2;
    
    // 0; 1; ... по убыванию первоочерёдности
    const static std::map<TaskPriority, size_t> priorities = 
        {
            {TaskPriority::High, 0}, 
            {TaskPriority::Normal, 1}
        };
}  // namespace dispatcher