// 
// This file is part of AT&C project which simulates virtual world of air traffic and ATC.
// Code licensing terms are available at https://github.com/felix-b/atc/blob/master/LICENSE
// 
#pragma once

#include <string>
#include <sstream>

#include "libworld.h"

using namespace std;

namespace world
{
    class SequentialManeuver : public Maneuver
    {
    private:
        shared_ptr<Maneuver> m_inProgressChild;
    public:
        SequentialManeuver(Type _type, const string& _id, const vector<shared_ptr<Maneuver>>& steps) :
            Maneuver(_type, _id, steps)
        {
        }
    public:
        void progressTo(chrono::microseconds timestamp) override
        {
            if (m_state == Maneuver::State::Finished)
            {
                return;
            }

            if (m_state == Maneuver::State::NotStarted)
            {
                m_inProgressChild = firstChild();
                m_startTimestamp = timestamp;
                m_state = Maneuver::State::InProgress;
            }

            while (true)
            {
                if (!m_inProgressChild)
                {
                    m_state = Maneuver::State::Finished;
                    m_finishTimestamp = timestamp;
                    break;
                }

                if (m_inProgressChild->state() != Maneuver::State::Finished)
                {
                    m_inProgressChild->progressTo(timestamp);

                    if (m_inProgressChild->state() != Maneuver::State::Finished)
                    {
                        break;
                    }
                }

                m_inProgressChild = m_inProgressChild->nextSibling();
            }
        }

        string getStatusString() const override
        {
            stringstream s;
            s << Maneuver::getStatusString();

            if (m_inProgressChild)
            {
                s << "[" << m_inProgressChild->getStatusString() << "]";
            }

            return s.str();
        }
    };

    class ParallelManeuver : public Maneuver
    {
    public:
        ParallelManeuver(Type _type, const string& _id, const vector<shared_ptr<Maneuver>>& _steps) :
            Maneuver(_type, _id, _steps)
        {
        }
    public:
        void progressTo(chrono::microseconds timestamp) override
        {
            switch (m_state)
            {
            case Maneuver::State::Finished:
                return;
            case Maneuver::State::NotStarted:
                m_startTimestamp = timestamp;
                break;
            }

            m_finishTimestamp = timestamp;
            bool allFinished = true;

            for (auto child = firstChild() ; !!child ; child = child->nextSibling())
            {
                if (child->state() != Maneuver::State::Finished)
                {
                    child->progressTo(timestamp);
                    if (child->state() != Maneuver::State::Finished)
                    {
                        allFinished = false;
                    }
                }
            }

            m_state = allFinished 
                ? Maneuver::State::Finished 
                : Maneuver::State::InProgress;
        }

        string getStatusString() const override
        {
            stringstream s;
            s << Maneuver::getStatusString();
            s << "(";

            bool isFirstChild = true;
            for (auto child = firstChild() ; child ; child = child->nextSibling())
            {
                if (!isFirstChild)
                {
                    s << "|";
                }
                s << child->getStatusString();
                isFirstChild = false;
            }

            s << ")";
            return s.str();
        }
    };

    template<class T>
    class AnimationManeuver : public Maneuver
    {
    public:
        typedef function<void(
            const T& startValue, 
            const T& endValue, 
            double progress, 
            T& result
        )> FormulaFunction;
        typedef function<void(const T& value, double progress)> ApplyFunction;
        typedef function<Maneuver::SemaphoreState(
            Maneuver::SemaphoreState previousState,
            chrono::microseconds closedStateTotalDuration
        )> SemaphoreFunction;
    private:
        T m_startValue;
        T m_endValue;
        T m_lastValue;
        chrono::microseconds m_duration;
        FormulaFunction m_formula;
        ApplyFunction m_apply;
        SemaphoreFunction m_semaphore;
        SemaphoreState m_lastSemaphoreState;
        chrono::microseconds m_semaphoreWaitDuration;
        chrono::microseconds m_lastElapsed;
    public:
        AnimationManeuver(
            const string& _id,
            const T& _startValue,
            const T& _endValue,
            chrono::microseconds _duration,
            FormulaFunction _formula,
            ApplyFunction _apply,
            SemaphoreFunction _semaphore = noopSemaphore
        ) : Maneuver(Maneuver::Type::Animation, _id, {}),
            m_startValue(_startValue),
            m_endValue(_endValue),
            m_duration(_duration),
            m_formula(_formula),
            m_apply(_apply),
            m_semaphore(_semaphore),
            m_lastSemaphoreState(SemaphoreState::Open),
            m_semaphoreWaitDuration(chrono::microseconds(0)),
            m_lastElapsed(chrono::microseconds(0))
        {
        }    
    public:
        void progressTo(chrono::microseconds timestamp)
        {
            if (m_state == Maneuver::State::NotStarted)
            {
                m_startTimestamp = timestamp;
                m_state = Maneuver::State::InProgress;
            }

            chrono::microseconds elapsed = timestamp - m_startTimestamp;
            chrono::microseconds deltaElapsed = elapsed - m_lastElapsed;
            m_lastElapsed = elapsed;

            m_lastSemaphoreState = m_semaphore(m_lastSemaphoreState, m_semaphoreWaitDuration);
            if (m_lastSemaphoreState == SemaphoreState::Closed)
            {
                m_semaphoreWaitDuration += deltaElapsed;
                return;
            }

            chrono::microseconds elapsedAnimation = elapsed - m_semaphoreWaitDuration;
            double progress = min(
                1.0, 
                (double)elapsedAnimation.count() / (double)m_duration.count());
            m_formula(m_startValue, m_endValue, progress, m_lastValue);
            m_apply(m_lastValue, progress);

            if (elapsedAnimation >= m_duration)
            {
                m_state = Maneuver::State::Finished;
                m_finishTimestamp = timestamp;
            }
        }
    public:
        static SemaphoreState noopSemaphore(SemaphoreState, chrono::microseconds)
        {
            return SemaphoreState::Open;
        }
    };

    class AwaitManeuver : public Maneuver
    {
    private:
        shared_ptr<HostServices> m_host;
        function<bool()> m_isReady;
    public:
        AwaitManeuver(shared_ptr<HostServices> _host, Maneuver::Type _type, const string& _id, function<bool()> _isReady) :
            Maneuver(_type, _id, {}),
            m_host(_host),
            m_isReady(_isReady)
        {
        }
    public:
        void progressTo(chrono::microseconds timestamp) override
        {
            if (m_state == Maneuver::State::NotStarted)
            {
                m_startTimestamp = timestamp;
                m_state = Maneuver::State::InProgress;
            }

            if (m_state == Maneuver::State::InProgress)
            {
                if (m_isReady())
                {
                    m_state = Maneuver::State::Finished;
                    m_finishTimestamp = timestamp;
                }
                if (!id().empty())
                {
                    logStatus(timestamp);
                }
            }
        }
    private:
        void logStatus(chrono::microseconds timestamp)
        {
            chrono::microseconds elapsed = timestamp - m_startTimestamp;
            if (m_state == Maneuver::State::Finished)
            {
                m_host->writeLog("AIPILO|AWAIT [%s] FINISHED in [%d] ms", id().c_str(), elapsed.count() / 1000);
            }
            else if (elapsed.count() > 0 && (elapsed.count() % 1000000) == 0)
            {
                m_host->writeLog("AIPILO|AWAIT [%s] in progress for [%d] sec", id().c_str(), elapsed.count() / 1000000);
            }
        }
    };

    class InstantActionManeuver : public Maneuver
    {
    private:
        function<void()> m_action;
    public:
        InstantActionManeuver(Maneuver::Type _type, const string& _id, function<void()> _action) :
            Maneuver(_type, _id, {}),
            m_action(_action)
        {
        }
    public:
        void progressTo(chrono::microseconds timestamp) override
        {
            if (m_state == Maneuver::State::NotStarted)
            {
                m_action();
                m_state = Maneuver::State::Finished;
            }
        }
    };

    class DeferredManeuver : public Maneuver
    {
    public:
        typedef function<shared_ptr<Maneuver>()> Factory;
    private:
        Factory m_factory;
        shared_ptr<Maneuver> m_actual;
    public:
        DeferredManeuver(
            Maneuver::Type _type, 
            const string& _id, 
            Factory _factory
        ) : Maneuver(_type, _id, {}),
            m_factory(_factory)
        {
        }
    public:
        bool isProxy() const override
        {
            return true;
        }
        void progressTo(chrono::microseconds timestamp) override
        {
            if (m_state == Maneuver::State::Finished)
            {
                return;
            }

            if (m_state == Maneuver::State::NotStarted)
            {
                m_actual = m_factory();
                m_startTimestamp = timestamp;
            }

            m_actual->progressTo(timestamp);
            m_state = m_actual->state();

            if (m_state == Maneuver::State::Finished)
            {
                m_finishTimestamp = timestamp;
            }
        }
        string getStatusString() const override
        {
            return m_actual ? m_actual->getStatusString() : "defer";
        }
    private:
        shared_ptr<Maneuver> unProxy() const override 
        { 
            return m_actual;
        }
    public:
        static shared_ptr<Maneuver> create(
            Maneuver::Type _type, 
            const string& _id,
            Factory _factory)
        {
            return shared_ptr<Maneuver>(new DeferredManeuver(_type, _id, _factory));
        }
    };
}
