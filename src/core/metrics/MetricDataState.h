/*
 * 模块说明：读数有效性与显示状态
 * 将数值与时间戳、有效采样次数一起判断，区分初读、正常、历史不足、过期和不支持。
 * 无效点可用 NaN 标记，让曲线断开；是否显示数值应以状态为准，不能只看默认的 0。
 */
#pragma once

#include <cmath>
#include <cstdint>
#include <cstddef>

/**
 * @brief 监控指标数据状态枚举 (MetricDataState)
 * 
 * 为什么需要数据状态枚举？
 * 在硬件监控中，硬件读数并不总是“有”或“无”。可能处于：刚启动还没读到、已就绪、
 * 硬件本身不支持、读数卡死超时（陈旧）、传感器断开连接、或采集点不足以画满历史曲线。
 * 状态机能让前端 UI 精准展示对应样式（如灰色骨架屏、正常数值、或横线“—”）。
 */
enum class MetricDataState {
    Reading,        ///< 正在初次读取中（刚启动或首次探针探测中）
    Ready,          ///< 数据正常就绪（已有有效采样且历史点充足）
    Unsupported,    ///< 硬件或驱动不支持此项指标（例如虚拟机或未开放权限的传感器）
    Stale,          ///< 数据已陈旧/超时（上次采集成功，但近期超过限定时长未更新，可能传感器休眠）
    Disconnected,   ///< 设备已断开连接（如移动硬盘或外置硬件拔出）
    Partial         ///< 部分就绪（已有读数，但 60 秒历史点尚未蓄满，曲线在累积阶段）
};

/**
 * @brief 单项指标采样状态跟踪器 (MetricSampleStatus)
 * 
 * 职责：
 * 1. 负责记录某项采样指标的当前状态、最后有效采样时间、最后尝试采样时间；
 * 2. 避免 UI 刷新导致虚假采样（区分“时间流逝”与“真正的新数据采样”）；
 * 3. 自动探测数据丢包（如果跳过了若干秒，自动在历史环形队列中插入 NAN 空缺占位，防止波形图连线失真）。
 */
struct MetricSampleStatus {
    MetricDataState state = MetricDataState::Reading; ///< 当前指标的状态，默认正在读取中
    uint64_t lastValidMs = 0;                         ///< 最近一次获取到“有效数值”的时间戳 (毫秒)
    uint64_t lastAttemptMs = 0;                       ///< 最近一次尝试执行采样的单调时间戳 (毫秒)

    /**
     * @brief 记录一次新的硬件采样值
     * 
     * @tparam History 环形历史缓冲区类型 (如 MetricHistory)
     * @param sample  采集到的新浮点数值（如果采集失败通常为 NAN 或非法值）
     * @param value   [输出参数] 存放最新有效值的变量引用
     * @param history [输出参数] 存放最近 60 点历史记录的环形队列
     * @param now     当前单调系统时间戳 (毫秒)
     */
    template<class History>
    void Record(float sample, float& value, History& history, uint64_t now) {
        // 如果本次尝试时间早于或等于上次时间，说明时钟未前进或并发重复调用，直接忽略
        if (lastAttemptMs && now <= lastAttemptMs) return;

        // 如果距离上次采样超过 1500 毫秒（预期采样周期是 1000 毫秒），说明发生了卡顿或跳帧丢包
        // 按秒数补偿在历史缓冲区中压入 NAN (非数)，在波形图上表现为折线中断，而不是错误地把前后两点连起来
        if (lastAttemptMs && now > lastAttemptMs + 1500) {
            const auto missed = (now - lastAttemptMs) / 1000 - 1;
            for (uint64_t i = 0; i < missed && i < History::kCapacity; ++i)
                history.Push(NAN);
        }
        lastAttemptMs = now;

        // 判断采到的浮点数是否有效且有限 (std::isfinite 排除 NaN 和无穷大 Inf)
        if (std::isfinite(sample)) {
            value = sample;
            lastValidMs = now;
            history.Push(sample);
            state = MetricDataState::Ready;
        } else {
            // 采样失败或硬件返回无效值，压入 NAN 并在历史上标记为陈旧或等待中
            history.Push(NAN);
            state = lastValidMs ? MetricDataState::Stale : MetricDataState::Reading;
        }
    }

    /**
     * @brief 根据时效性综合决策最终展现状态
     * 
     * @param valid      当前指标是否有有效值
     * @param validCount 历史队列中有效采样点的总数 (例如满容量是 60 个点)
     * @param now        当前单调时间戳 (毫秒)
     * @return MetricDataState 最终判定状态
     */
    MetricDataState Resolve(bool valid, size_t validCount, uint64_t now) const {
        // 如果硬件本身不支持或已拔出，维持该终态，不自动恢复
        if (state == MetricDataState::Unsupported || state == MetricDataState::Disconnected) return state;

        // 如果明确标记为 Stale，或者超过 3000 毫秒没有有效数据注入，判定为读数挂起（陈旧）
        if (state == MetricDataState::Stale || (lastValidMs && now > lastValidMs + 3000)) return MetricDataState::Stale;

        // 如果当前没有有效值，处于初始化读取状态
        if (!valid) return MetricDataState::Reading;

        // 如果采样点不足 60 个（例如刚开机跑了 10 秒），属于 Partial（部分就绪）状态
        return validCount < 60 ? MetricDataState::Partial : MetricDataState::Ready;
    }
};
