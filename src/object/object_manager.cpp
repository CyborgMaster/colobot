/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2014, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsiteс.ch; http://colobot.info; http://github.com/colobot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://gnu.org/licenses
 */

#include "object/object_manager.h"


#include "math/all.h"

#include "object/object.h"
#include "object/object_create_exception.h"
#include "object/object_create_params.h"
#include "object/object_factory.h"
#include "object/old_object.h"

#include "object/auto/auto.h"

#include "physics/physics.h"

#include <algorithm>


template<> CObjectManager* CSingleton<CObjectManager>::m_instance = nullptr;


CObjectManager::CObjectManager(Gfx::CEngine* engine,
                               Gfx::CTerrain* terrain,
                               Gfx::COldModelManager* oldModelManager,
                               Gfx::CModelManager* modelManager,
                               Gfx::CParticle* particle)
  : m_objectFactory(new CObjectFactory(engine, terrain, oldModelManager, modelManager, particle))
  , m_nextId(0)
  , m_shouldCleanRemovedObjects(false)
{
}

CObjectManager::~CObjectManager()
{
}

bool CObjectManager::DeleteObject(CObject* instance)
{
    assert(instance != nullptr);

    // TODO: temporarily...
    auto oldObj = dynamic_cast<COldObject*>(instance);
    if (oldObj != nullptr)
        oldObj->DeleteObject();

    auto it = m_objects.find(instance->GetID());
    if (it != m_objects.end())
    {
        it->second.reset();
        m_shouldCleanRemovedObjects = true;
        return true;
    }

    return false;
}

void CObjectManager::CleanRemovedObjects()
{
    auto it = m_objects.begin();
    if (it != m_objects.end())
    {
        if (it->second == nullptr)
            it = m_objects.erase(it);
    }

    m_shouldCleanRemovedObjects = false;
}

void CObjectManager::DeleteAllObjects()
{
    for (auto& it : m_objects)
    {
        // TODO: temporarily...
        auto oldObj = dynamic_cast<COldObject*>(it.second.get());
        if (oldObj != nullptr)
        {
            bool all = true;
            oldObj->DeleteObject(all);
        }
    }

    m_objects.clear();

    m_nextId = 0;
}

CObject* CObjectManager::GetObjectById(unsigned int id)
{
    if (m_objects.count(id) == 0) return nullptr;
    return m_objects[id].get();
}

CObject* CObjectManager::GetObjectByRank(unsigned int id)
{
    if (id >= m_objects.size()) return nullptr;
    auto it = m_objects.begin();
    for (unsigned int i = 0; i < id; i++, ++it);
    return it->second.get();
}

CObject* CObjectManager::CreateObject(ObjectCreateParams params)
{
    if (params.id < 0)
    {
        params.id = m_nextId;
        m_nextId++;
    }

    assert(m_objects.find(params.id) == m_objects.end());

    auto objectUPtr = m_objectFactory->CreateObject(params);

    if (objectUPtr == nullptr)
        throw CObjectCreateException("Something went wrong in CObjectFactory", params.type);

    CObject* objectPtr = objectUPtr.get();

    m_objects[params.id] = std::move(objectUPtr);

    return objectPtr;
}

CObject* CObjectManager::CreateObject(Math::Vector pos,
                                      float angle,
                                      ObjectType type,
                                      float power,
                                      float zoom,
                                      float height,
                                      bool trainer,
                                      bool toy,
                                      int option,
                                      int team,
                                      int id)
{
    ObjectCreateParams params;
    params.pos = pos;
    params.angle = angle;
    params.type = type;
    params.power = power;
    params.zoom = zoom;
    params.height = height;
    params.trainer = trainer;
    params.toy = toy;
    params.option = option;
    params.team = team;
    params.id = id;

    return CreateObject(params);
}

std::vector<CObject*> CObjectManager::GetObjectsOfTeam(int team)
{
    std::vector<CObject*> result;
    for (CObject* object : GetAllObjects())
    {
        if (object->GetTeam() == team)
        {
            result.push_back(object);
        }
    }
    return result;
}

bool CObjectManager::TeamExists(int team)
{
    if(team == 0) return true;

    for (CObject* object : GetAllObjects())
    {
        if (!object->GetActive())
            continue;

        if (object->GetTeam() == team)
            return true;
    }
    return false;
}

void CObjectManager::DestroyTeam(int team)
{
    assert(team != 0);

    for (CObject* object : GetAllObjects())
    {
        if (object->GetTeam() == team)
            object->ExplodeObject(ExplosionType::Bang, 1.0f);
    }
}

int CObjectManager::CountObjectsImplementing(ObjectInterfaceType interface)
{
    int count = 0;
    for (CObject* object : GetAllObjects())
    {
        if (object->Implements(interface))
            count++;
    }
    return count;
}

CObject* CObjectManager::Radar(CObject* pThis, ObjectType type, float angle, float focus, float minDist, float maxDist, bool furthest, RadarFilter filter, bool cbotTypes)
{
    std::vector<ObjectType> types;
    if (type != OBJECT_NULL)
        types.push_back(type);
    return Radar(pThis, types, angle, focus, minDist, maxDist, furthest, filter, cbotTypes);
}

CObject* CObjectManager::Radar(CObject* pThis, std::vector<ObjectType> type, float angle, float focus, float minDist, float maxDist, bool furthest, RadarFilter filter, bool cbotTypes)
{
    Math::Vector iPos;
    float iAngle;
    if (pThis != nullptr)
    {
        iPos   = pThis->GetPosition();
        iAngle = pThis->GetRotationY();
        iAngle = Math::NormAngle(iAngle);  // 0..2*Math::PI
    }
    else
    {
        iPos   = Math::Vector();
        iAngle = 0.0f;
    }
    return Radar(pThis, iPos, iAngle, type, angle, focus, minDist, maxDist, furthest, filter, cbotTypes);
}

CObject* CObjectManager::Radar(CObject* pThis, Math::Vector thisPosition, float thisAngle, ObjectType type, float angle, float focus, float minDist, float maxDist, bool furthest, RadarFilter filter, bool cbotTypes)
{
    std::vector<ObjectType> types;
    if (type != OBJECT_NULL)
        types.push_back(type);
    return Radar(pThis, thisPosition, thisAngle, types, angle, focus, minDist, maxDist, furthest, filter, cbotTypes);
}

CObject* CObjectManager::Radar(CObject* pThis, Math::Vector thisPosition, float thisAngle, std::vector<ObjectType> type, float angle, float focus, float minDist, float maxDist, bool furthest, RadarFilter filter, bool cbotTypes)
{
    CObject     *pObj, *pBest;
    CPhysics*   physics;
    Math::Vector    iPos, oPos;
    float       best, iAngle, d, a;
    ObjectType  oType;

    minDist *= g_unit;
    maxDist *= g_unit;

    iPos   = thisPosition;
    iAngle = thisAngle+angle;
    iAngle = Math::NormAngle(iAngle);  // 0..2*Math::PI

    int filter_team = filter & 0xFF;
    RadarFilter filter_flying = static_cast<RadarFilter>(filter & (FILTER_ONLYLANDING | FILTER_ONLYFLYING));
    RadarFilter filter_enemy = static_cast<RadarFilter>(filter & (FILTER_FRIENDLY | FILTER_ENEMY | FILTER_NEUTRAL));

    if ( !furthest )  best = 100000.0f;
    else              best = 0.0f;
    pBest = nullptr;
    for ( auto it = m_objects.begin() ; it != m_objects.end() ; ++it )
    {
        pObj = it->second.get();
        if ( pObj == pThis )  continue; // pThis may be nullptr but it doesn't matter

        if (pObj == nullptr) continue;
        if (IsObjectBeingTransported(pObj))  continue;
        if ( !pObj->GetActive() )  continue;
        if ( pObj->GetProxyActivate() )  continue;

        oType = pObj->GetType();

        if (cbotTypes)
        {
            // TODO: handle this differently (new class describing types? CObjectType::GetBaseType()?)
            if ( oType == OBJECT_RUINmobilew2 ||
                oType == OBJECT_RUINmobilet1 ||
                oType == OBJECT_RUINmobilet2 ||
                oType == OBJECT_RUINmobiler1 ||
                oType == OBJECT_RUINmobiler2 )
            {
                oType = OBJECT_RUINmobilew1;  // any ruin
            }

            if ( oType == OBJECT_SCRAP2 ||
                oType == OBJECT_SCRAP3 ||
                oType == OBJECT_SCRAP4 ||
                oType == OBJECT_SCRAP5 )  // wastes?
            {
                oType = OBJECT_SCRAP1;  // any waste
            }

            if ( oType == OBJECT_BARRIER2 ||
                oType == OBJECT_BARRIER3 )  // barriers?
            {
                oType = OBJECT_BARRIER1;  // any barrier
            }
            // END OF TODO
        }

        if ( std::find(type.begin(), type.end(), oType) == type.end() && type.size() > 0 )  continue;

        if ( (oType == OBJECT_TOTO || oType == OBJECT_CONTROLLER) && type.size() == 0 )  continue; // allow OBJECT_TOTO and OBJECT_CONTROLLER only if explicitly asked in type parameter

        if ( filter_flying == FILTER_ONLYLANDING )
        {
            physics = pObj->GetPhysics();
            if ( physics != nullptr && !physics->GetLand() )  continue;
        }
        if ( filter_flying == FILTER_ONLYFLYING )
        {
            physics = pObj->GetPhysics();
            if ( physics != nullptr && physics->GetLand() )  continue;
        }

        if ( filter_team != 0 && pObj->GetTeam() != filter_team )
            continue;

        if( pThis != nullptr )
        {
            RadarFilter enemy = FILTER_NONE;
            if ( pObj->GetTeam() == 0 ) enemy = static_cast<RadarFilter>(enemy | FILTER_NEUTRAL);
            if ( pObj->GetTeam() != 0 && pObj->GetTeam() == pThis->GetTeam() ) enemy = static_cast<RadarFilter>(enemy | FILTER_FRIENDLY);
            if ( pObj->GetTeam() != 0 && pObj->GetTeam() != pThis->GetTeam() ) enemy = static_cast<RadarFilter>(enemy | FILTER_ENEMY);
            if ( filter_enemy != 0 && (filter_enemy & enemy) == 0 ) continue;
        }

        oPos = pObj->GetPosition();
        d = Math::DistanceProjected(iPos, oPos);
        if ( d < minDist || d > maxDist )  continue;  // too close or too far?

        a = Math::RotateAngle(oPos.x-iPos.x, iPos.z-oPos.z);  // CW !
        if ( Math::TestAngle(a, iAngle-focus/2.0f, iAngle+focus/2.0f) || focus >= Math::PI*2.0f )
        {
            if ( (!furthest && d < best) ||
                (furthest && d > best) )
            {
                best = d;
                pBest = pObj;
            }
        }
    }

    return pBest;
}

CObject*  CObjectManager::FindNearest(CObject* pThis, ObjectType type, float maxDist, bool cbotTypes)
{
    return Radar(pThis, type, 0.0f, Math::PI*2.0f, 0.0f, maxDist, false, FILTER_NONE, cbotTypes);
}

CObject*  CObjectManager::FindNearest(CObject* pThis, std::vector<ObjectType> type, float maxDist, bool cbotTypes)
{
    return Radar(pThis, type, 0.0f, Math::PI*2.0f, 0.0f, maxDist, false, FILTER_NONE, cbotTypes);
}

CObject*  CObjectManager::FindNearest(CObject* pThis, Math::Vector thisPosition, ObjectType type, float maxDist, bool cbotTypes)
{
    return Radar(pThis, thisPosition, 0.0f, type, 0.0f, Math::PI*2.0f, 0.0f, maxDist, false, FILTER_NONE, cbotTypes);
}

CObject*  CObjectManager::FindNearest(CObject* pThis, Math::Vector thisPosition, std::vector<ObjectType> type, float maxDist, bool cbotTypes)
{
    return Radar(pThis, thisPosition, 0.0f, type, 0.0f, Math::PI*2.0f, 0.0f, maxDist, false, FILTER_NONE, cbotTypes);
}
