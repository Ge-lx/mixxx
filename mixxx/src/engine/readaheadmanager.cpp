// readaheadmanager.cpp
// Created 8/2/2009 by RJ Ryan (rryan@mit.edu)

#include "engine/readaheadmanager.h"

#include "mathstuff.h"
#include "engine/enginecontrol.h"
#include "cachingreader.h"


ReadAheadManager::ReadAheadManager(CachingReader* pReader) :
    m_iCurrentPosition(0),
    m_pReader(pReader) {
}

ReadAheadManager::~ReadAheadManager() {
}

int ReadAheadManager::getNextSamples(double dRate, CSAMPLE* buffer,
                                     int requested_samples) {
    Q_ASSERT(even(requested_samples));
    
    bool in_reverse = dRate < 0;
    int start_sample = m_iCurrentPosition;
    int samples_needed = requested_samples;
    CSAMPLE* base_buffer = buffer;
    
    

    // A loop will only limit the amount we can read in one shot.
    QPair<int, double> next_loop = getSoonestTrigger(in_reverse,
                                                     m_iCurrentPosition);
    if (next_loop.second != kNoTrigger) {
        int samples_available;
        if (in_reverse) {
            samples_available = m_iCurrentPosition - next_loop.second;
        } else {
            samples_available = next_loop.second - m_iCurrentPosition;
        }
        samples_needed = math_max(0, math_min(samples_needed,
                                              samples_available));
    }

    if (in_reverse) {
        start_sample = m_iCurrentPosition - samples_needed;
        if (start_sample < 0) {
            samples_needed += start_sample;
            start_sample = 0;
        }
    }

    // Sanity checks
    Q_ASSERT(start_sample >= 0);
    Q_ASSERT(samples_needed >= 0);
    
    int samples_read = m_pReader->read(start_sample, samples_needed,
                                       base_buffer);

    // Increment or decrement current read-ahead position
    if (in_reverse) {
        m_iCurrentPosition -= samples_read;
    } else {
        m_iCurrentPosition += samples_read;
    }

    // Activate on this trigger if necessary
    if (next_loop.second != kNoTrigger) {
        double loop_target = m_sEngineControls[next_loop.first]->
            getTrigger(dRate,
                       m_iCurrentPosition,
                       0, 0);
        
        if ((in_reverse && m_iCurrentPosition <= loop_target) ||
            (!in_reverse && m_iCurrentPosition >= loop_target)) {
            m_iCurrentPosition = loop_target;
        }
    }

    // Reverse the samples in-place
    if (in_reverse) {
        // TODO(rryan) pull this into MixxxUtil or something
        CSAMPLE temp1, temp2;
        for (int j = 0; j < samples_read/2; j++) {
            const int endpos = samples_read-1-j-1;
            temp1 = base_buffer[j];
            temp2 = base_buffer[j+1];
            base_buffer[j] = base_buffer[endpos];
            base_buffer[j+1] = base_buffer[endpos+1];
            base_buffer[endpos] = temp1;
            base_buffer[endpos+1] = temp2;
        }
    }
    
    return samples_read;
}

void ReadAheadManager::addEngineControl(EngineControl* pControl) {
    Q_ASSERT(pControl);
    m_sEngineControls.append(pControl);
}

void ReadAheadManager::setNewPlaypos(int iNewPlaypos) {
    m_iCurrentPosition = iNewPlaypos;
}

void ReadAheadManager::notifySeek(int iSeekPosition) {
    m_iCurrentPosition = iSeekPosition;
}

QPair<int, double> ReadAheadManager::getSoonestTrigger(double dRate,
                                                       int iCurrentSample) {
    bool in_reverse = dRate < 0;
    double next_trigger = kNoTrigger;
    int next_trigger_index = -1;
    int i;
    for (int i = 0; i < m_sEngineControls.size(); ++i) {
        // TODO(rryan) eh.. this interface is likely to change so dont sweat the
        // last 2 parameters for now, nothing currently uses them
        double trigger = m_sEngineControls[i]->nextTrigger(dRate, iCurrentSample,
                                                           0, 0);
        bool trigger_active = (trigger != kNoTrigger &&
                               ((in_reverse && trigger <= iCurrentSample) ||
                                (!in_reverse && trigger >= iCurrentSample)));
        if (trigger_active &&
            (next_trigger == kNoTrigger ||
             (in_reverse && trigger > next_trigger) ||
             (!in_reverse && trigger < next_trigger))) {
            
            next_trigger = trigger;
            next_trigger_index = i;
        }
    }
    return qMakePair(next_trigger_index, next_trigger);
}
