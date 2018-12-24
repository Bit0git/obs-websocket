/**
 * obs-websocket
 * Copyright (C) 2016-2017	Stéphane Lepin <stephane.lepin@gmail.com>
 * Copyright (C) 2017	Mikhail Swift <https://github.com/mikhailswift>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>
 */

#include <obs-data.h>

#include "Config.h"
#include "Utils.h"

#include "WSRequestHandler.h"

QHash<QString, void(*)(WSRequestHandler*)> WSRequestHandler::messageMap {
	{ "GetVersion", WSRequestHandler::HandleGetVersion },

	{ "SetHeartbeat", WSRequestHandler::HandleSetHeartbeat },

	{ "SetFilenameFormatting", WSRequestHandler::HandleSetFilenameFormatting },
	{ "GetFilenameFormatting", WSRequestHandler::HandleGetFilenameFormatting },

	{ "SetCurrentScene", WSRequestHandler::HandleSetCurrentScene },
	{ "GetCurrentScene", WSRequestHandler::HandleGetCurrentScene },
	{ "GetSceneList", WSRequestHandler::HandleGetSceneList },

	{ "SetSourceRender", WSRequestHandler::HandleSetSceneItemRender }, // Retrocompat
	{ "SetSceneItemRender", WSRequestHandler::HandleSetSceneItemRender },
	{ "SetSceneItemPosition", WSRequestHandler::HandleSetSceneItemPosition },
	{ "SetSceneItemTransform", WSRequestHandler::HandleSetSceneItemTransform },
	{ "SetSceneItemCrop", WSRequestHandler::HandleSetSceneItemCrop },
	{ "GetSceneItemProperties", WSRequestHandler::HandleGetSceneItemProperties },
	{ "SetSceneItemProperties", WSRequestHandler::HandleSetSceneItemProperties },
	{ "ResetSceneItem", WSRequestHandler::HandleResetSceneItem },
	{ "DeleteSceneItem", WSRequestHandler::HandleDeleteSceneItem },
	{ "DuplicateSceneItem", WSRequestHandler::HandleDuplicateSceneItem },

	{ "GetStreamingStatus", WSRequestHandler::HandleGetStreamingStatus },
	{ "StartStopStreaming", WSRequestHandler::HandleStartStopStreaming },
	{ "StartStopRecording", WSRequestHandler::HandleStartStopRecording },
	{ "StartStreaming", WSRequestHandler::HandleStartStreaming },
	{ "StopStreaming", WSRequestHandler::HandleStopStreaming },
	{ "StartRecording", WSRequestHandler::HandleStartRecording },
	{ "StopRecording", WSRequestHandler::HandleStopRecording },

	{ "StartStopReplayBuffer", WSRequestHandler::HandleStartStopReplayBuffer },
	{ "StartReplayBuffer", WSRequestHandler::HandleStartReplayBuffer },
	{ "StopReplayBuffer", WSRequestHandler::HandleStopReplayBuffer },
	{ "SaveReplayBuffer", WSRequestHandler::HandleSaveReplayBuffer },

	{ "SetRecordingFolder", WSRequestHandler::HandleSetRecordingFolder },
	{ "GetRecordingFolder", WSRequestHandler::HandleGetRecordingFolder },

	{ "GetTransitionList", WSRequestHandler::HandleGetTransitionList },
	{ "GetCurrentTransition", WSRequestHandler::HandleGetCurrentTransition },
	{ "SetCurrentTransition", WSRequestHandler::HandleSetCurrentTransition },
	{ "SetTransitionDuration", WSRequestHandler::HandleSetTransitionDuration },
	{ "GetTransitionDuration", WSRequestHandler::HandleGetTransitionDuration },

	{ "SetVolume", WSRequestHandler::HandleSetVolume },
	{ "GetVolume", WSRequestHandler::HandleGetVolume },
	{ "ToggleMute", WSRequestHandler::HandleToggleMute },
	{ "SetMute", WSRequestHandler::HandleSetMute },
	{ "GetMute", WSRequestHandler::HandleGetMute },
	{ "SetSyncOffset", WSRequestHandler::HandleSetSyncOffset },
	{ "GetSyncOffset", WSRequestHandler::HandleGetSyncOffset },
	{ "GetSpecialSources", WSRequestHandler::HandleGetSpecialSources },
	{ "GetSourcesList", WSRequestHandler::HandleGetSourcesList },
	{ "GetSourceTypesList", WSRequestHandler::HandleGetSourceTypesList },
	{ "GetSourceSettings", WSRequestHandler::HandleGetSourceSettings },
	{ "SetSourceSettings", WSRequestHandler::HandleSetSourceSettings },

	{ "GetSourceFilters", WSRequestHandler::HandleGetSourceFilters },
	{ "AddFilterToSource", WSRequestHandler::HandleAddFilterToSource },
	{ "RemoveFilterFromSource", WSRequestHandler::HandleRemoveFilterFromSource },
	{ "ReorderSourceFilter", WSRequestHandler::HandleReorderSourceFilter },
	{ "MoveSourceFilter", WSRequestHandler::HandleMoveSourceFilter },
	{ "SetSourceFilterSettings", WSRequestHandler::HandleSetSourceFilterSettings },

	{ "SetCurrentSceneCollection", WSRequestHandler::HandleSetCurrentSceneCollection },
	{ "GetCurrentSceneCollection", WSRequestHandler::HandleGetCurrentSceneCollection },
	{ "ListSceneCollections", WSRequestHandler::HandleListSceneCollections },

	{ "SetCurrentProfile", WSRequestHandler::HandleSetCurrentProfile },
	{ "GetCurrentProfile", WSRequestHandler::HandleGetCurrentProfile },
	{ "ListProfiles", WSRequestHandler::HandleListProfiles },

	{ "SetStreamSettings", WSRequestHandler::HandleSetStreamSettings },
	{ "GetStreamSettings", WSRequestHandler::HandleGetStreamSettings },
	{ "SaveStreamSettings", WSRequestHandler::HandleSaveStreamSettings },

	{ "GetStudioModeStatus", WSRequestHandler::HandleGetStudioModeStatus },
	{ "GetPreviewScene", WSRequestHandler::HandleGetPreviewScene },
	{ "SetPreviewScene", WSRequestHandler::HandleSetPreviewScene },
	{ "TransitionToProgram", WSRequestHandler::HandleTransitionToProgram },
	{ "EnableStudioMode", WSRequestHandler::HandleEnableStudioMode },
	{ "DisableStudioMode", WSRequestHandler::HandleDisableStudioMode },
	{ "ToggleStudioMode", WSRequestHandler::HandleToggleStudioMode },

	{ "SetTextGDIPlusProperties", WSRequestHandler::HandleSetTextGDIPlusProperties },
	{ "GetTextGDIPlusProperties", WSRequestHandler::HandleGetTextGDIPlusProperties },

	{ "SetTextFreetype2Properties", WSRequestHandler::HandleSetTextFreetype2Properties },
	{ "GetTextFreetype2Properties", WSRequestHandler::HandleGetTextFreetype2Properties },

	{ "GetBrowserSourceProperties", WSRequestHandler::HandleGetBrowserSourceProperties },
	{ "SetBrowserSourceProperties", WSRequestHandler::HandleSetBrowserSourceProperties }
};

QSet<QString> WSRequestHandler::authNotRequired {
	"GetVersion",
	"GetAuthRequired",
	"Authenticate"
};

WSRequestHandler::WSRequestHandler() :
	_messageId(0),
	_requestType(""),
	data(nullptr),
{
}

void WSRequestHandler::processIncomingMessage(QString textMessage) {
	QByteArray msgData = textMessage.toUtf8();
	const char* msg = msgData.constData();

	data = obs_data_create_from_json(msg);
	if (!data) {
		if (!msg)
			msg = "<null pointer>";

		blog(LOG_ERROR, "invalid JSON payload received for '%s'", msg);
		SendErrorResponse("invalid JSON payload");
		return;
	}

	if (Config::Current()->DebugEnabled) {
		blog(LOG_DEBUG, "Request >> '%s'", msg);
	}

	if (!hasField("request-type")
		|| !hasField("message-id"))
	{
		SendErrorResponse("missing request parameters");
		return;
	}

	_requestType = obs_data_get_string(data, "request-type");
	_messageId = obs_data_get_string(data, "message-id");

	void (*handlerFunc)(WSRequestHandler*) = (messageMap[_requestType]);

	if (handlerFunc != nullptr)
		handlerFunc(this);
	else
		SendErrorResponse("invalid request type");
}

WSRequestHandler::~WSRequestHandler() {
}

void WSRequestHandler::SendOKResponse(obs_data_t* additionalFields) {
	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "status", "ok");
	obs_data_set_string(response, "message-id", _messageId);

	if (additionalFields)
		obs_data_apply(response, additionalFields);

	SendResponse(response);
}

void WSRequestHandler::SendErrorResponse(const char* errorMessage) {
	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "status", "error");
	obs_data_set_string(response, "error", errorMessage);
	obs_data_set_string(response, "message-id", _messageId);

	SendResponse(response);
}

void WSRequestHandler::SendErrorResponse(obs_data_t* additionalFields) {
	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "status", "error");
	obs_data_set_string(response, "message-id", _messageId);

	if (additionalFields)
		obs_data_set_obj(response, "error", additionalFields);

	SendResponse(response);
}

void WSRequestHandler::SendResponse(obs_data_t* response)  {
	assert(_response.isNull());
	_response = obs_data_get_json(response);

	if (Config::Current()->DebugEnabled)
		blog(LOG_DEBUG, "Response << '%s'", _response.toUtf8().constData());
}

bool WSRequestHandler::hasField(QString name) {
	if (!data || name.isEmpty() || name.isNull())
		return false;

	return obs_data_has_user_value(data, name.toUtf8());
}
