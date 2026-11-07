# 连接匹配本次 AXF 的目标并暂停后使用；此文件只定义读取命令。
define nwatch_snapshot
    p g_nwatchRtos
    p g_nwatchFault
    p xTickCount
    p uxCurrentNumberOfTasks
    p/x pxCurrentTCB
end
document nwatch_snapshot
读取诊断快照和内核变量，不调用目标函数、不写入目标内存。
end
