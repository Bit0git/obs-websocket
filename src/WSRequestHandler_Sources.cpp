#include <QString>
#include "Utils.h"

#include "WSRequestHandler.h"

/**
* @typedef {Object} `Source` An OBS Scene Item.
* @property {Number} `cy`
* @property {Number} `cx`
* @property {String} `name` The name of this Scene Item.
* @property {Boolean} `render` Whether or not this Scene Item is set to "visible".
* @property {Number} `source_cx`
* @property {Number} `source_cy`
* @property {String} `type` Source type. Value is one of the following: "input", "filter", "transition", "scene" or "unknown"
* @property {Number} `volume`
* @property {Number} `x`
* @property {Number} `y`
*/

/**
* List all sources available in the running OBS instance
*
* @return {Array<Object>} `sources` Array of sources
* @return {String} `sources.*.name` Unique source name
* @return {String} `sources.*.typeId` Non-unique source internal type (a.k.a type id)
* @return {String} `sources.*.type` Source type. Value is one of the following: "input", "filter", "transition", "scene" or "unknown"
*
* @api requests
* @name GetSourcesList
* @category sources
* @since 4.3.0
*/
void WSRequestHandler::HandleGetSourcesList(WSRequestHandler* req)
{
	OBSDataArrayAutoRelease sourcesArray = obs_data_array_create();

	auto sourceEnumProc = [](void* privateData, obs_source_t* source) -> bool {
		obs_data_array_t* sourcesArray = (obs_data_array_t*)privateData;

		OBSDataAutoRelease sourceData = obs_data_create();
		obs_data_set_string(sourceData, "name", obs_source_get_name(source));
		obs_data_set_string(sourceData, "typeId", obs_source_get_id(source));

		QString typeString = "";
		enum obs_source_type sourceType = obs_source_get_type(source);
		switch (sourceType) {
		case OBS_SOURCE_TYPE_INPUT:
			typeString = "input";
			break;

		case OBS_SOURCE_TYPE_FILTER:
			typeString = "filter";
			break;

		case OBS_SOURCE_TYPE_TRANSITION:
			typeString = "transition";
			break;

		case OBS_SOURCE_TYPE_SCENE:
			typeString = "scene";
			break;

		default:
			typeString = "unknown";
			break;
		}
		obs_data_set_string(sourceData, "type", typeString.toUtf8());

		obs_data_array_push_back(sourcesArray, sourceData);
		return true;
	};
	obs_enum_sources(sourceEnumProc, sourcesArray);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "sources", sourcesArray);
	req->SendOKResponse(response);
}

/**
* Get a list of all available sources types
*
* @return {Array<Object>} `ids` Array of source types
* @return {String} `ids.*.typeId` Non-unique internal source type ID
* @return {String} `ids.*.displayName` Display name of the source type
* @return {String} `ids.*.type` Type. Value is one of the following: "input", "filter", "transition" or "other"
* @return {Object} `ids.*.defaultSettings` Default settings of this source type
* @return {Object} `ids.*.caps` Source type capabilities
* @return {Boolean} `ids.*.caps.isAsync` True if source of this type provide frames asynchronously
* @return {Boolean} `ids.*.caps.hasVideo` True if sources of this type provide video
* @return {Boolean} `ids.*.caps.hasAudio` True if sources of this type provide audio
* @return {Boolean} `ids.*.caps.canInteract` True if interaction with this sources of this type is possible
* @return {Boolean} `ids.*.caps.isComposite` True if sources of this type composite one or more sub-sources
* @return {Boolean} `ids.*.caps.doNotDuplicate` True if sources of this type should not be fully duplicated
* @return {Boolean} `ids.*.caps.doNotSelfMonitor` True if sources of this type may cause a feedback loop if it's audio is monitored and shouldn't be
*
* @api requests
* @name GetSourcesTypesList
* @category sources
* @since 4.3.0
*/
void WSRequestHandler::HandleGetSourceTypesList(WSRequestHandler* req)
{
	OBSDataArrayAutoRelease idsArray = obs_data_array_create();

	const char* id;
	size_t idx = 0;

	QHash<QString, QString> idTypes;

	idx = 0;
	while (obs_enum_input_types(idx++, &id)) {
		idTypes.insert(id, "input");
	}

	idx = 0;
	while (obs_enum_filter_types(idx++, &id)) {
		idTypes.insert(id, "filter");
	}

	idx = 0;
	while (obs_enum_transition_types(idx++, &id)) {
		idTypes.insert(id, "transition");
	}

	idx = 0;
	while (obs_enum_source_types(idx++, &id)) {
		OBSDataAutoRelease item = obs_data_create();

		obs_data_set_string(item, "typeId", id);
		obs_data_set_string(item, "displayName", obs_source_get_display_name(id));
		obs_data_set_string(item, "type", idTypes.value(id, "other").toUtf8());

		uint32_t caps = obs_get_source_output_flags(id);
		OBSDataAutoRelease capsData = obs_data_create();
		obs_data_set_bool(capsData, "isAsync", caps & OBS_SOURCE_ASYNC);
		obs_data_set_bool(capsData, "hasVideo", caps & OBS_SOURCE_VIDEO);
		obs_data_set_bool(capsData, "hasAudio", caps & OBS_SOURCE_AUDIO);
		obs_data_set_bool(capsData, "canInteract", caps & OBS_SOURCE_INTERACTION);
		obs_data_set_bool(capsData, "isComposite", caps & OBS_SOURCE_COMPOSITE);
		obs_data_set_bool(capsData, "doNotDuplicate", caps & OBS_SOURCE_DO_NOT_DUPLICATE);
		obs_data_set_bool(capsData, "doNotSelfMonitor", caps & OBS_SOURCE_DO_NOT_SELF_MONITOR);
		obs_data_set_bool(capsData, "isDeprecated", caps & OBS_SOURCE_DEPRECATED);

		obs_data_set_obj(item, "caps", capsData);

		OBSDataAutoRelease defaultSettings = obs_get_source_defaults(id);
		obs_data_set_obj(item, "defaultSettings", defaultSettings);

		obs_data_array_push_back(idsArray, item);
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "types", idsArray);
	req->SendOKResponse(response);
}

/**
* Get the volume of the specified source.
*
* @param {String} `source` Source name.
*
* @return {String} `name` Source name.
* @return {double} `volume` Volume of the source. Between `0.0` and `1.0`.
* @return {boolean} `muted` Indicates whether the source is muted.
*
* @api requests
* @name GetVolume
* @category sources
* @since 4.0.0
*/
void WSRequestHandler::HandleGetVolume(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "name", obs_source_get_name(source));
	obs_data_set_double(response, "volume", obs_source_get_volume(source));
	obs_data_set_bool(response, "muted", obs_source_muted(source));

	req->SendOKResponse(response);
}

/**
 * Set the volume of the specified source.
 *
 * @param {String} `source` Source name.
 * @param {double} `volume` Desired volume. Must be between `0.0` and `1.0`.
 *
 * @api requests
 * @name SetVolume
 * @category sources
 * @since 4.0.0
 */
 void WSRequestHandler::HandleSetVolume(WSRequestHandler* req)
 {
	if (!req->hasField("source") || !req->hasField("volume"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	float sourceVolume = obs_data_get_double(req->data, "volume");

	if (sourceName.isEmpty() || sourceVolume < 0.0 || sourceVolume > 1.0) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	obs_source_set_volume(source, sourceVolume);
	req->SendOKResponse();
}

/**
* Get the mute status of a specified source.
*
* @param {String} `source` Source name.
*
* @return {String} `name` Source name.
* @return {boolean} `muted` Mute status of the source.
*
* @api requests
* @name GetMute
* @category sources
* @since 4.0.0
*/
void WSRequestHandler::HandleGetMute(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "name", obs_source_get_name(source));
	obs_data_set_bool(response, "muted", obs_source_muted(source));

	req->SendOKResponse(response);
}

/**
 * Sets the mute status of a specified source.
 *
 * @param {String} `source` Source name.
 * @param {boolean} `mute` Desired mute status.
 *
 * @api requests
 * @name SetMute
 * @category sources
 * @since 4.0.0
 */
void WSRequestHandler::HandleSetMute(WSRequestHandler* req)
{
	if (!req->hasField("source") || !req->hasField("mute")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	bool mute = obs_data_get_bool(req->data, "mute");

	if (sourceName.isEmpty()) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	obs_source_set_muted(source, mute);
	req->SendOKResponse();
}

/**
* Inverts the mute status of a specified source.
*
* @param {String} `source` Source name.
*
* @api requests
* @name ToggleMute
* @category sources
* @since 4.0.0
*/
void WSRequestHandler::HandleToggleMute(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	obs_source_set_muted(source, !obs_source_muted(source));
	req->SendOKResponse();
}

/**
 * Set the audio sync offset of a specified source.
 *
 * @param {String} `source` Source name.
 * @param {int} `offset` The desired audio sync offset (in nanoseconds).
 *
 * @api requests
 * @name SetSyncOffset
 * @category sources
 * @since 4.2.0
 */
void WSRequestHandler::HandleSetSyncOffset(WSRequestHandler* req)
{
	if (!req->hasField("source") || !req->hasField("offset")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	int64_t sourceSyncOffset = (int64_t)obs_data_get_int(req->data, "offset");

	if (sourceName.isEmpty() || sourceSyncOffset < 0) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	obs_source_set_sync_offset(source, sourceSyncOffset);
	req->SendOKResponse();
}

/**
 * Get the audio sync offset of a specified source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `name` Source name.
 * @return {int} `offset` The audio sync offset (in nanoseconds).
 *
 * @api requests
 * @name GetSyncOffset
 * @category sources
 * @since 4.2.0
 */
void WSRequestHandler::HandleGetSyncOffset(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "name", obs_source_get_name(source));
	obs_data_set_int(response, "offset", obs_source_get_sync_offset(source));

	req->SendOKResponse(response);
}

/**
* Get settings of the specified source
*
* @param {String} `sourceName` Source name.
* @param {String (optional)} `sourceType` Type of the specified source. Useful for type-checking if you expect a specific settings schema.
*
* @return {String} `sourceName` Source name
* @return {String} `sourceType` Type of the specified source
* @return {Object} `sourceSettings` Source settings (varies between source types, may require some probing around).
*
* @api requests
* @name GetSourceSettings
* @category sources
* @since 4.3.0
*/
void WSRequestHandler::HandleGetSourceSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	if (req->hasField("sourceType")) {
		QString actualSourceType = obs_source_get_id(source);
		QString requestedType = obs_data_get_string(req->data, "sourceType");

		if (actualSourceType != requestedType) {
			req->SendErrorResponse("specified source exists but is not of expected type");
			return;
		}
	}

	OBSDataAutoRelease sourceSettings = obs_source_get_settings(source);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "sourceName", obs_source_get_name(source));
	obs_data_set_string(response, "sourceType", obs_source_get_id(source));
	obs_data_set_obj(response, "sourceSettings", sourceSettings);

	req->SendOKResponse(response);
}

/**
* Set settings of the specified source.
*
* @param {String} `sourceName` Source name.
* @param {String (optional)} `sourceType` Type of the specified source. Useful for type-checking to avoid settings a set of settings incompatible with the actual source's type.
* @param {Object} `sourceSettings` Source settings (varies between source types, may require some probing around).
*
* @return {String} `sourceName` Source name
* @return {String} `sourceType` Type of the specified source
* @return {Object} `sourceSettings` Updated source settings
*
* @api requests
* @name SetSourceSettings
* @category sources
* @since 4.3.0
*/
void WSRequestHandler::HandleSetSourceSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("sourceSettings")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	if (req->hasField("sourceType")) {
		QString actualSourceType = obs_source_get_id(source);
		QString requestedType = obs_data_get_string(req->data, "sourceType");

		if (actualSourceType != requestedType) {
			req->SendErrorResponse("specified source exists but is not of expected type");
			return;
		}
	}

	OBSDataAutoRelease currentSettings = obs_source_get_settings(source);
	OBSDataAutoRelease newSettings = obs_data_get_obj(req->data, "sourceSettings");

	OBSDataAutoRelease sourceSettings = obs_data_create();
	obs_data_apply(sourceSettings, currentSettings);
	obs_data_apply(sourceSettings, newSettings);

	obs_source_update(source, sourceSettings);
	obs_source_update_properties(source);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "sourceName", obs_source_get_name(source));
	obs_data_set_string(response, "sourceType", obs_source_get_id(source));
	obs_data_set_obj(response, "sourceSettings", sourceSettings);

	req->SendOKResponse(response);
}

/**
 * Get the current properties of a Text GDI Plus source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `source` Source name.
 * @return {String} `align` Text Alignment ("left", "center", "right").
 * @return {int} `bk-color` Background color.
 * @return {int} `bk-opacity` Background opacity (0-100).
 * @return {boolean} `chatlog` Chat log.
 * @return {int} `chatlog_lines` Chat log lines.
 * @return {int} `color` Text color.
 * @return {boolean} `extents` Extents wrap.
 * @return {int} `extents_cx` Extents cx.
 * @return {int} `extents_cy` Extents cy.
 * @return {String} `file` File path name.
 * @return {boolean} `read_from_file` Read text from the specified file.
 * @return {Object} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @return {String} `font.face` Font face.
 * @return {int} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @return {int} `font.size` Font text size.
 * @return {String} `font.style` Font Style (unknown function).
 * @return {boolean} `gradient` Gradient enabled.
 * @return {int} `gradient_color` Gradient color.
 * @return {float} `gradient_dir` Gradient direction.
 * @return {int} `gradient_opacity` Gradient opacity (0-100).
 * @return {boolean} `outline` Outline.
 * @return {int} `outline_color` Outline color.
 * @return {int} `outline_size` Outline size.
 * @return {int} `outline_opacity` Outline opacity (0-100).
 * @return {String} `text` Text content to be displayed.
 * @return {String} `valign` Text vertical alignment ("top", "center", "bottom").
 * @return {boolean} `vertical` Vertical text enabled.
 *
 * @api requests
 * @name GetTextGDIPlusProperties
 * @category sources
 * @since 4.1.0
 */
 void WSRequestHandler::HandleGetTextGDIPlusProperties(WSRequestHandler* req)
 {
	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_gdiplus") {
		req->SendErrorResponse("not a text gdi plus source");
		return;
	}

	OBSDataAutoRelease response = obs_source_get_settings(source);
	obs_data_set_string(response, "source", obs_source_get_name(source));

	req->SendOKResponse(response);
}

/**
 * Set the current properties of a Text GDI Plus source.
 *
 * @param {String} `source` Name of the source.
 * @param {String (optional)} `align` Text Alignment ("left", "center", "right").
 * @param {int (optional)} `bk-color` Background color.
 * @param {int (optional)} `bk-opacity` Background opacity (0-100).
 * @param {boolean (optional)} `chatlog` Chat log.
 * @param {int (optional)} `chatlog_lines` Chat log lines.
 * @param {int (optional)} `color` Text color.
 * @param {boolean (optional)} `extents` Extents wrap.
 * @param {int (optional)} `extents_cx` Extents cx.
 * @param {int (optional)} `extents_cy` Extents cy.
 * @param {String (optional)} `file` File path name.
 * @param {boolean (optional)} `read_from_file` Read text from the specified file.
 * @param {Object (optional)} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @param {String (optional)} `font.face` Font face.
 * @param {int (optional)} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @param {int (optional)} `font.size` Font text size.
 * @param {String (optional)} `font.style` Font Style (unknown function).
 * @param {boolean (optional)} `gradient` Gradient enabled.
 * @param {int (optional)} `gradient_color` Gradient color.
 * @param {float (optional)} `gradient_dir` Gradient direction.
 * @param {int (optional)} `gradient_opacity` Gradient opacity (0-100).
 * @param {boolean (optional)} `outline` Outline.
 * @param {int (optional)} `outline_color` Outline color.
 * @param {int (optional)} `outline_size` Outline size.
 * @param {int (optional)} `outline_opacity` Outline opacity (0-100).
 * @param {String (optional)} `text` Text content to be displayed.
 * @param {String (optional)} `valign` Text vertical alignment ("top", "center", "bottom").
 * @param {boolean (optional)} `vertical` Vertical text enabled.
 * @param {boolean (optional)} `render` Visibility of the scene item.
 *
 * @api requests
 * @name SetTextGDIPlusProperties
 * @category sources
 * @since 4.1.0
 */
void WSRequestHandler::HandleSetTextGDIPlusProperties(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_gdiplus") {
		req->SendErrorResponse("not a text gdi plus source");
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (req->hasField("align")) {
		obs_data_set_string(settings, "align", obs_data_get_string(req->data, "align"));
	}

	if (req->hasField("bk_color")) {
		obs_data_set_int(settings, "bk_color", obs_data_get_int(req->data, "bk_color"));
	}

	if (req->hasField("bk-opacity")) {
		obs_data_set_int(settings, "bk_opacity", obs_data_get_int(req->data, "bk_opacity"));
	}

	if (req->hasField("chatlog")) {
		obs_data_set_bool(settings, "chatlog", obs_data_get_bool(req->data, "chatlog"));
	}

	if (req->hasField("chatlog_lines")) {
		obs_data_set_int(settings, "chatlog_lines", obs_data_get_int(req->data, "chatlog_lines"));
	}

	if (req->hasField("color")) {
		obs_data_set_int(settings, "color", obs_data_get_int(req->data, "color"));
	}

	if (req->hasField("extents")) {
		obs_data_set_bool(settings, "extents", obs_data_get_bool(req->data, "extents"));
	}

	if (req->hasField("extents_wrap")) {
		obs_data_set_bool(settings, "extents_wrap", obs_data_get_bool(req->data, "extents_wrap"));
	}

	if (req->hasField("extents_cx")) {
		obs_data_set_int(settings, "extents_cx", obs_data_get_int(req->data, "extents_cx"));
	}

	if (req->hasField("extents_cy")) {
		obs_data_set_int(settings, "extents_cy", obs_data_get_int(req->data, "extents_cy"));
	}

	if (req->hasField("file")) {
		obs_data_set_string(settings, "file", obs_data_get_string(req->data, "file"));
	}

	if (req->hasField("font")) {
		OBSDataAutoRelease font_obj = obs_data_get_obj(settings, "font");
		if (font_obj) {
			OBSDataAutoRelease req_font_obj = obs_data_get_obj(req->data, "font");

			if (obs_data_has_user_value(req_font_obj, "face")) {
				obs_data_set_string(font_obj, "face", obs_data_get_string(req_font_obj, "face"));
			}

			if (obs_data_has_user_value(req_font_obj, "flags")) {
				obs_data_set_int(font_obj, "flags", obs_data_get_int(req_font_obj, "flags"));
			}

			if (obs_data_has_user_value(req_font_obj, "size")) {
				obs_data_set_int(font_obj, "size", obs_data_get_int(req_font_obj, "size"));
			}

			if (obs_data_has_user_value(req_font_obj, "style")) {
				obs_data_set_string(font_obj, "style", obs_data_get_string(req_font_obj, "style"));
			}
		}
	}

	if (req->hasField("gradient")) {
		obs_data_set_bool(settings, "gradient", obs_data_get_bool(req->data, "gradient"));
	}

	if (req->hasField("gradient_color")) {
		obs_data_set_int(settings, "gradient_color", obs_data_get_int(req->data, "gradient_color"));
	}

	if (req->hasField("gradient_dir")) {
		obs_data_set_double(settings, "gradient_dir", obs_data_get_double(req->data, "gradient_dir"));
	}

	if (req->hasField("gradient_opacity")) {
		obs_data_set_int(settings, "gradient_opacity", obs_data_get_int(req->data, "gradient_opacity"));
	}

	if (req->hasField("outline")) {
		obs_data_set_bool(settings, "outline", obs_data_get_bool(req->data, "outline"));
	}

	if (req->hasField("outline_size")) {
		obs_data_set_int(settings, "outline_size", obs_data_get_int(req->data, "outline_size"));
	}

	if (req->hasField("outline_color")) {
		obs_data_set_int(settings, "outline_color", obs_data_get_int(req->data, "outline_color"));
	}

	if (req->hasField("outline_opacity")) {
		obs_data_set_int(settings, "outline_opacity", obs_data_get_int(req->data, "outline_opacity"));
	}

	if (req->hasField("read_from_file")) {
		obs_data_set_bool(settings, "read_from_file", obs_data_get_bool(req->data, "read_from_file"));
	}

	if (req->hasField("text")) {
		obs_data_set_string(settings, "text", obs_data_get_string(req->data, "text"));
	}

	if (req->hasField("valign")) {
		obs_data_set_string(settings, "valign", obs_data_get_string(req->data, "valign"));
	}

	if (req->hasField("vertical")) {
		obs_data_set_bool(settings, "vertical", obs_data_get_bool(req->data, "vertical"));
	}

	obs_source_update(source, settings);

	req->SendOKResponse();
}

/**
 * Get the current properties of a Text Freetype 2 source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `source` Source name
 * @return {int} `color1` Gradient top color.
 * @return {int} `color2` Gradient bottom color.
 * @return {int} `custom_width` Custom width (0 to disable).
 * @return {boolean} `drop_shadow` Drop shadow.
 * @return {Object} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @return {String} `font.face` Font face.
 * @return {int} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @return {int} `font.size` Font text size.
 * @return {String} `font.style` Font Style (unknown function).
 * @return {boolean} `from_file` Read text from the specified file.
 * @return {boolean} `log_mode` Chat log.
 * @return {boolean} `outline` Outline.
 * @return {String} `text` Text content to be displayed.
 * @return {String} `text_file` File path.
 * @return {boolean} `word_wrap` Word wrap.
 *
 * @api requests
 * @name GetTextFreetype2Properties
 * @category sources
 * @since 4.5.0
 */
void WSRequestHandler::HandleGetTextFreetype2Properties(WSRequestHandler* req)
{
	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_ft2_source") {
		req->SendErrorResponse("not a freetype 2 source");
		return;
	}

	OBSDataAutoRelease response = obs_source_get_settings(source);
	obs_data_set_string(response, "source", sourceName);

	req->SendOKResponse(response);
}

/**
 * Set the current properties of a Text Freetype 2 source.
 *
 * @param {String} `source` Source name.
 * @param {int (optional)} `color1` Gradient top color.
 * @param {int (optional)} `color2` Gradient bottom color.
 * @param {int (optional)} `custom_width` Custom width (0 to disable).
 * @param {boolean (optional)} `drop_shadow` Drop shadow.
 * @param {Object (optional)} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @param {String (optional)} `font.face` Font face.
 * @param {int (optional)} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @param {int (optional)} `font.size` Font text size.
 * @param {String (optional)} `font.style` Font Style (unknown function).
 * @param {boolean (optional)} `from_file` Read text from the specified file.
 * @param {boolean (optional)} `log_mode` Chat log.
 * @param {boolean (optional)} `outline` Outline.
 * @param {String (optional)} `text` Text content to be displayed.
 * @param {String (optional)} `text_file` File path.
 * @param {boolean (optional)} `word_wrap` Word wrap.
 *
 * @api requests
 * @name SetTextFreetype2Properties
 * @category sources
 * @since 4.5.0
 */
void WSRequestHandler::HandleSetTextFreetype2Properties(WSRequestHandler* req)
{
    const char* sourceName = obs_data_get_string(req->data, "source");
    if (!sourceName) {
        req->SendErrorResponse("invalid request parameters");
        return;
    }

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_ft2_source") {
		req->SendErrorResponse("not text freetype 2 source");
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (req->hasField("color1")) {
		obs_data_set_int(settings, "color1", obs_data_get_int(req->data, "color1"));
	}

	if (req->hasField("color2")) {
		obs_data_set_int(settings, "color2", obs_data_get_int(req->data, "color2"));
	}

	if (req->hasField("custom_width")) {
		obs_data_set_int(settings, "custom_width", obs_data_get_int(req->data, "custom_width"));
	}

	if (req->hasField("drop_shadow")) {
		obs_data_set_bool(settings, "drop_shadow", obs_data_get_bool(req->data, "drop_shadow"));
	}

	if (req->hasField("font")) {
		OBSDataAutoRelease font_obj = obs_data_get_obj(settings, "font");
		if (font_obj) {
			OBSDataAutoRelease req_font_obj = obs_data_get_obj(req->data, "font");

			if (obs_data_has_user_value(req_font_obj, "face")) {
				obs_data_set_string(font_obj, "face", obs_data_get_string(req_font_obj, "face"));
			}

			if (obs_data_has_user_value(req_font_obj, "flags")) {
				obs_data_set_int(font_obj, "flags", obs_data_get_int(req_font_obj, "flags"));
			}

			if (obs_data_has_user_value(req_font_obj, "size")) {
				obs_data_set_int(font_obj, "size", obs_data_get_int(req_font_obj, "size"));
			}

			if (obs_data_has_user_value(req_font_obj, "style")) {
				obs_data_set_string(font_obj, "style", obs_data_get_string(req_font_obj, "style"));
			}
		}
	}

	if (req->hasField("from_file")) {
		obs_data_set_bool(settings, "from_file", obs_data_get_bool(req->data, "from_file"));
	}

	if (req->hasField("log_mode")) {
		obs_data_set_bool(settings, "log_mode", obs_data_get_bool(req->data, "log_mode"));
	}

	if (req->hasField("outline")) {
		obs_data_set_bool(settings, "outline", obs_data_get_bool(req->data, "outline"));
	}

	if (req->hasField("text")) {
		obs_data_set_string(settings, "text", obs_data_get_string(req->data, "text"));
	}

	if (req->hasField("text_file")) {
		obs_data_set_string(settings, "text_file", obs_data_get_string(req->data, "text_file"));
	}

	if (req->hasField("word_wrap")) {
		obs_data_set_bool(settings, "word_wrap", obs_data_get_bool(req->data, "word_wrap"));
	}

	obs_source_update(source, settings);

	req->SendOKResponse();
}

/**
 * Get the current properties of a Text Freetype 2 source.
 *
 * @param {String (optional)} `scene-name` Name of the scene to retrieve. Defaults to the current scene.
 * @param {String} `source` Name of the source.
 *
 * @return {int} `color1` Gradient top color.
 * @return {int} `color2` Gradient bottom color.
 * @return {int} `custom_width` Custom width (0 to disable).
 * @return {boolean} `drop_shadow` Drop shadow.
 * @return {Object} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @return {String} `font.face` Font face.
 * @return {int} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @return {int} `font.size` Font text size.
 * @return {String} `font.style` Font Style (unknown function).
 * @return {boolean} `from_file` Read text from the specified file.
 * @return {boolean} `log_mode` Chat log.
 * @return {boolean} `outline` Outline.
 * @return {String} `text` Text content to be displayed.
 * @return {String} `text_file` File path.
 * @return {boolean} `word_wrap` Word wrap.
 * @return {boolean} `render` Visibility of the scene item.
 *
 * @api requests
 * @name GetTextFreetype2Properties
 * @category sources
 * @since 4.x.x
 */
 void WSRequestHandler::HandleGetTextFreetype2Properties(WSRequestHandler* req) {
	// TODO: source settings are independent of any scene, so there's no need
	// to target a specific scene

	const char* itemName = obs_data_get_string(req->data, "source");
	if (!itemName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	const char* sceneName = obs_data_get_string(req->data, "scene-name");
	OBSSourceAutoRelease scene = Utils::GetSceneFromNameOrCurrent(sceneName);
	if (!scene) {
		req->SendErrorResponse("requested scene doesn't exist");
		return;
	}

	OBSSceneItemAutoRelease sceneItem = Utils::GetSceneItemFromName(scene, itemName);
	if (sceneItem) {
		OBSSource sceneItemSource = obs_sceneitem_get_source(sceneItem);
		const char* sceneItemSourceId = obs_source_get_id(sceneItemSource);

		if (strcmp(sceneItemSourceId, "text_ft2_source") == 0) {
			OBSDataAutoRelease response = obs_source_get_settings(sceneItemSource);
			obs_data_set_string(response, "source", itemName);
			obs_data_set_string(response, "scene-name", sceneName);
			obs_data_set_bool(response, "render",
				obs_sceneitem_visible(sceneItem));

			req->SendOKResponse(response);
		} else {
			req->SendErrorResponse("not freetype 2 source");
		}
	} else {
		req->SendErrorResponse("specified scene item doesn't exist");
	}
}

/**
 * Set the current properties of a Text Freetype 2 source.
 *
 * @param {String (optional)} `scene-name` Name of the scene to retrieve. Defaults to the current scene.
 * @param {String} `source` Name of the source.
 * @param {int (optional)} `color1` Gradient top color.
 * @param {int (optional)} `color2` Gradient bottom color.
 * @param {int (optional)} `custom_width` Custom width (0 to disable).
 * @param {boolean (optional)} `drop_shadow` Drop shadow.
 * @param {Object (optional)} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @param {String (optional)} `font.face` Font face.
 * @param {int (optional)} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @param {int (optional)} `font.size` Font text size.
 * @param {String (optional)} `font.style` Font Style (unknown function).
 * @param {boolean (optional)} `from_file` Read text from the specified file.
 * @param {boolean (optional)} `log_mode` Chat log.
 * @param {boolean (optional)} `outline` Outline.
 * @param {String (optional)} `text` Text content to be displayed.
 * @param {String (optional)} `text_file` File path.
 * @param {boolean (optional)} `word_wrap` Word wrap.
 * @param {boolean (optional)} `render` Visibility of the scene item.
 *
 * @api requests
 * @name SetTextFreetype2Properties
 * @category sources
 * @since 4.x.x
 */
void WSRequestHandler::HandleSetTextFreetype2Properties(WSRequestHandler* req) {
	// TODO: source settings are independent of any scene, so there's no need
	// to target a specific scene

	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* itemName = obs_data_get_string(req->data, "source");
	if (!itemName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	const char* sceneName = obs_data_get_string(req->data, "scene-name");
	OBSSourceAutoRelease scene = Utils::GetSceneFromNameOrCurrent(sceneName);
	if (!scene) {
		req->SendErrorResponse("requested scene doesn't exist");
		return;
	}

	OBSSceneItemAutoRelease sceneItem = Utils::GetSceneItemFromName(scene, itemName);
	if (sceneItem) {
		OBSSource sceneItemSource = obs_sceneitem_get_source(sceneItem);
		const char* sceneItemSourceId = obs_source_get_id(sceneItemSource);

		if (strcmp(sceneItemSourceId, "text_ft2_source") == 0) {
			OBSDataAutoRelease settings = obs_source_get_settings(sceneItemSource);

			if (req->hasField("color1")) {
				obs_data_set_int(settings, "color1",
					obs_data_get_int(req->data, "color1"));
			}

			if (req->hasField("color2")) {
				obs_data_set_int(settings, "color2",
					obs_data_get_int(req->data, "color2"));
			}

			if (req->hasField("custom_width")) {
				obs_data_set_int(settings, "custom_width",
					obs_data_get_int(req->data, "custom_width"));
			}

			if (req->hasField("drop_shadow")) {
				obs_data_set_bool(settings, "drop_shadow",
					obs_data_get_bool(req->data, "drop_shadow"));
			}

			if (req->hasField("font")) {
				OBSDataAutoRelease font_obj = obs_data_get_obj(settings, "font");
				if (font_obj) {
					OBSDataAutoRelease req_font_obj = obs_data_get_obj(req->data, "font");

					if (obs_data_has_user_value(req_font_obj, "face")) {
						obs_data_set_string(font_obj, "face",
							obs_data_get_string(req_font_obj, "face"));
					}

					if (obs_data_has_user_value(req_font_obj, "flags")) {
						obs_data_set_int(font_obj, "flags",
							obs_data_get_int(req_font_obj, "flags"));
					}

					if (obs_data_has_user_value(req_font_obj, "size")) {
						obs_data_set_int(font_obj, "size",
							obs_data_get_int(req_font_obj, "size"));
					}

					if (obs_data_has_user_value(req_font_obj, "style")) {
						obs_data_set_string(font_obj, "style",
							obs_data_get_string(req_font_obj, "style"));
					}
				}
			}

			if (req->hasField("from_file")) {
				obs_data_set_bool(settings, "from_file",
					obs_data_get_bool(req->data, "from_file"));
			}

			if (req->hasField("log_mode")) {
				obs_data_set_bool(settings, "log_mode",
					obs_data_get_bool(req->data, "log_mode"));
			}

			if (req->hasField("outline")) {
				obs_data_set_bool(settings, "outline",
					obs_data_get_bool(req->data, "outline"));
			}

			if (req->hasField("text")) {
				obs_data_set_string(settings, "text",
					obs_data_get_string(req->data, "text"));
			}

			if (req->hasField("text_file")) {
				obs_data_set_string(settings, "text_file",
					obs_data_get_string(req->data, "text_file"));
			}

			if (req->hasField("word_wrap")) {
				obs_data_set_bool(settings, "word_wrap",
					obs_data_get_bool(req->data, "word_wrap"));
			}

			obs_source_update(sceneItemSource, settings);

			if (req->hasField("render")) {
				obs_sceneitem_set_visible(sceneItem,
					obs_data_get_bool(req->data, "render"));
			}

			req->SendOKResponse();
		} else {
			req->SendErrorResponse("not text freetype 2 source");
		}
	} else {
		req->SendErrorResponse("specified scene item doesn't exist");
	}
}

/**
 * Get current properties for a Browser Source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `source` Source name.
 * @return {boolean} `is_local_file` Indicates that a local file is in use.
 * @return {String} `local_file` file path.
 * @return {String} `url` Url.
 * @return {String} `css` CSS to inject.
 * @return {int} `width` Width.
 * @return {int} `height` Height.
 * @return {int} `fps` Framerate.
 * @return {boolean} `shutdown` Indicates whether the source should be shutdown when not visible.
 *
 * @api requests
 * @name GetBrowserSourceProperties
 * @category sources
 * @since 4.1.0
 */
void WSRequestHandler::HandleGetBrowserSourceProperties(WSRequestHandler* req)
{
	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "browser_source") {
		req->SendErrorResponse("not a browser source");
		return;
	}

	OBSDataAutoRelease response = obs_source_get_settings(source);
	obs_data_set_string(response, "source", obs_source_get_name(source));

	req->SendOKResponse(response);
}

/**
 * Set current properties for a Browser Source.
 *
 * @param {String} `source` Name of the source.
 * @param {boolean (optional)} `is_local_file` Indicates that a local file is in use.
 * @param {String (optional)} `local_file` file path.
 * @param {String (optional)} `url` Url.
 * @param {String (optional)} `css` CSS to inject.
 * @param {int (optional)} `width` Width.
 * @param {int (optional)} `height` Height.
 * @param {int (optional)} `fps` Framerate.
 * @param {boolean (optional)} `shutdown` Indicates whether the source should be shutdown when not visible.
 * @param {boolean (optional)} `render` Visibility of the scene item.
 *
 * @api requests
 * @name SetBrowserSourceProperties
 * @category sources
 * @since 4.1.0
 */
void WSRequestHandler::HandleSetBrowserSourceProperties(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		req->SendErrorResponse("invalid request parameters");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	QString sourceId = obs_source_get_id(source);
	if(sourceId != "browser_source") {
		req->SendErrorResponse("not a browser source");
		return;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (req->hasField("restart_when_active")) {
		obs_data_set_bool(settings, "restart_when_active", obs_data_get_bool(req->data, "restart_when_active"));
	}

	if (req->hasField("shutdown")) {
		obs_data_set_bool(settings, "shutdown", obs_data_get_bool(req->data, "shutdown"));
	}

	if (req->hasField("is_local_file")) {
		obs_data_set_bool(settings, "is_local_file", obs_data_get_bool(req->data, "is_local_file"));
	}

	if (req->hasField("local_file")) {
		obs_data_set_string(settings, "local_file", obs_data_get_string(req->data, "local_file"));
	}

	if (req->hasField("url")) {
		obs_data_set_string(settings, "url", obs_data_get_string(req->data, "url"));
	}

	if (req->hasField("css")) {
		obs_data_set_string(settings, "css", obs_data_get_string(req->data, "css"));
	}

	if (req->hasField("width")) {
		obs_data_set_int(settings, "width", obs_data_get_int(req->data, "width"));
	}

	if (req->hasField("height")) {
		obs_data_set_int(settings, "height", obs_data_get_int(req->data, "height"));
	}

	if (req->hasField("fps")) {
		obs_data_set_int(settings, "fps", obs_data_get_int(req->data, "fps"));
	}

	obs_source_update(source, settings);

	req->SendOKResponse();
}

/**
 * Deletes a scene item.
 *
 * @param {String (optional)} `scene` Name of the scene the source belongs to. Defaults to the current scene.
 * @param {Object} `item` item to delete (required)
 * @param {String} `item.name` name of the scene item (prefer `id`, including both is acceptable).
 * @param {int} `item.id` id of the scene item.
 *
 * @api requests
 * @name DeleteSceneItem
 * @category sources
 * @since unreleased
 */
void WSRequestHandler::HandleDeleteSceneItem(WSRequestHandler* req) {
	if (!req->hasField("item")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sceneName = obs_data_get_string(req->data, "scene");
	OBSSourceAutoRelease scene = Utils::GetSceneFromNameOrCurrent(sceneName);
	if (!scene) {
		req->SendErrorResponse("requested scene doesn't exist");
		return;
	}

	OBSDataAutoRelease item = obs_data_get_obj(req->data, "item");
	OBSSceneItemAutoRelease sceneItem = Utils::GetSceneItemFromItem(scene, item);
	if (!sceneItem) {
		req->SendErrorResponse("item with id/name combination not found in specified scene");
		return;
	}

	obs_sceneitem_remove(sceneItem);
	req->SendOKResponse();
}

struct DuplicateSceneItemData {
	obs_sceneitem_t *referenceItem;
	obs_source_t *fromSource;
	obs_sceneitem_t *newItem;
};

static void DuplicateSceneItem(void *_data, obs_scene_t *scene) {
	DuplicateSceneItemData *data = (DuplicateSceneItemData *)_data;
	data->newItem = obs_scene_add(scene, data->fromSource);
	obs_sceneitem_set_visible(data->newItem, obs_sceneitem_visible(data->referenceItem));
}

/**
 * Duplicates a scene item.
 *
 * @param {String (optional)} `fromScene` Name of the scene to copy the item from. Defaults to the current scene.
 * @param {String (optional)} `toScene` Name of the scene to create the item in. Defaults to the current scene.
 * @param {Object} `item` item to delete (required)
 * @param {String} `item.name` name of the scene item (prefer `id`, including both is acceptable).
 * @param {int} `item.id` id of the scene item.
 *
 * @api requests
 * @name DuplicateSceneItem
 * @category sources
 * @since unreleased
 */
void WSRequestHandler::HandleDuplicateSceneItem(WSRequestHandler* req) {
	if (!req->hasField("item")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* fromSceneName = obs_data_get_string(req->data, "fromScene");
	OBSSourceAutoRelease fromScene = Utils::GetSceneFromNameOrCurrent(fromSceneName);
	if (!fromScene) {
		req->SendErrorResponse("requested fromScene doesn't exist");
		return;
	}

	const char* toSceneName = obs_data_get_string(req->data, "toScene");
	OBSSourceAutoRelease toScene = Utils::GetSceneFromNameOrCurrent(toSceneName);
	if (!toScene) {
		req->SendErrorResponse("requested toScene doesn't exist");
		return;
	}

	OBSDataAutoRelease item = obs_data_get_obj(req->data, "item");
	OBSSceneItemAutoRelease referenceItem = Utils::GetSceneItemFromItem(fromScene, item);
	if (!referenceItem) {
		req->SendErrorResponse("item with id/name combination not found in specified scene");
		return;
	}

	DuplicateSceneItemData data;
	data.fromSource = obs_sceneitem_get_source(referenceItem);
	data.referenceItem = referenceItem;

	obs_enter_graphics();
	obs_scene_atomic_update(obs_scene_from_source(toScene), DuplicateSceneItem, &data);
	obs_leave_graphics();

	obs_sceneitem_t *newItem = data.newItem;

	if (!newItem) {
		req->SendErrorResponse("Error duplicating scene item");
	}
	OBSDataAutoRelease responseData = obs_data_create();
	OBSDataAutoRelease itemData = obs_data_create();
	obs_data_set_int(itemData, "id", obs_sceneitem_get_id(newItem));
	obs_data_set_string(itemData, "name", obs_source_get_name(obs_sceneitem_get_source(newItem)));
	obs_data_set_obj(responseData, "item", itemData);
	obs_data_set_string(responseData, "scene", obs_source_get_name(toScene));
	req->SendOKResponse(responseData);
}

/**
 * Get configured special sources like Desktop Audio and Mic/Aux sources.
 *
 * @return {String (optional)} `desktop-1` Name of the first Desktop Audio capture source.
 * @return {String (optional)} `desktop-2` Name of the second Desktop Audio capture source.
 * @return {String (optional)} `mic-1` Name of the first Mic/Aux input source.
 * @return {String (optional)} `mic-2` Name of the second Mic/Aux input source.
 * @return {String (optional)} `mic-3` Name of the third Mic/Aux input source.
 *
 * @api requests
 * @name GetSpecialSources
 * @category sources
 * @since 4.1.0
 */
 void WSRequestHandler::HandleGetSpecialSources(WSRequestHandler* req)
 {
	OBSDataAutoRelease response = obs_data_create();

	QMap<const char*, int> sources;
	sources["desktop-1"] = 1;
	sources["desktop-2"] = 2;
	sources["mic-1"] = 3;
	sources["mic-2"] = 4;
	sources["mic-3"] = 5;

	QMapIterator<const char*, int> i(sources);
	while (i.hasNext()) {
		i.next();

		const char* id = i.key();
		OBSSourceAutoRelease source = obs_get_output_source(i.value());
		if (source) {
			obs_data_set_string(response, id, obs_source_get_name(source));
		}
	}

	req->SendOKResponse(response);
}

/**
* List filters applied to a source
*
* @param {String} `sourceName` Source name
*
* @return {Array<Object>} `filters` List of filters for the specified source
* @return {String} `filters.*.type` Filter type
* @return {String} `filters.*.name` Filter name
* @return {Object} `filters.*.settings` Filter settings
*
* @api requests
* @name GetSourceFilters
* @category sources
* @since 4.5.0
*/
void WSRequestHandler::HandleGetSourceFilters(WSRequestHandler* req)
{
	if (!req->hasField("sourceName")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSDataArrayAutoRelease filters = obs_data_array_create();
	obs_source_enum_filters(source, [](obs_source_t *parent, obs_source_t *child, void *param)
	{
		OBSDataAutoRelease filter = obs_data_create();
		obs_data_set_string(filter, "type", obs_source_get_id(child));
		obs_data_set_string(filter, "name", obs_source_get_name(child));
		obs_data_set_obj(filter, "settings", obs_source_get_settings(child));
		obs_data_array_push_back((obs_data_array_t*)param, filter);

	}, filters);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "filters", filters);
	req->SendOKResponse(response);
}

/**
* Add a new filter to a source. Available source types along with their settings properties are available from `GetSourceTypesList`.
*
* @param {String} `sourceName` Name of the source on which the filter is added
* @param {String} `filterName` Name of the new filter
* @param {String} `filterType` Filter type
* @param {Object} `filterSettings` Filter settings
*
* @api requests
* @name AddFilterToSource
* @category sources
* @since 4.5.0
*/
void WSRequestHandler::HandleAddFilterToSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") ||
		!req->hasField("filterType") || !req->hasField("filterSettings"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	const char* filterType = obs_data_get_string(req->data, "filterType");
	OBSDataAutoRelease filterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease existingFilter = obs_source_get_filter_by_name(source, filterName);
	if (existingFilter) {
		req->SendErrorResponse("filter name already taken");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_create_private(filterType, filterName, filterSettings);
	if (!filter) {
		req->SendErrorResponse("filter creation failed");
		return;
	}
	if (obs_source_get_type(filter) != OBS_SOURCE_TYPE_FILTER) {
		req->SendErrorResponse("invalid filter type");
		return;
	}

	obs_source_filter_add(source, filter);

	req->SendOKResponse();
}

/**
* Remove a filter from a source
*
* @param {String} `sourceName` Name of the source from which the specified filter is removed
* @param {String} `filterName` Name of the filter to remove
*
* @api requests
* @name RemoveFilterFromSource
* @category sources
* @since 4.5.0
*/
void WSRequestHandler::HandleRemoveFilterFromSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_source_filter_remove(source, filter);

	req->SendOKResponse();
}

/**
* Move a filter in the chain (absolute index positioning)
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reorder
* @param {Integer} `newIndex` Desired position of the filter in the chain
*
* @api requests
* @name ReorderSourceFilter
* @category sources
* @since 4.5.0
*/
void WSRequestHandler::HandleReorderSourceFilter(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("newIndex")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	int newIndex = obs_data_get_int(req->data, "newIndex");

	if (newIndex < 0) {
		req->SendErrorResponse("invalid index");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		req->SendErrorResponse("specified filter doesn't exist");
	}

	struct filterSearch {
		int i;
		int filterIndex;
		obs_source_t* filter;
	};
	struct filterSearch ctx = { 0, 0, filter };
	obs_source_enum_filters(source, [](obs_source_t *parent, obs_source_t *child, void *param)
	{
		struct filterSearch* ctx = (struct filterSearch*)param;
		if (child == ctx->filter) {
			ctx->filterIndex = ctx->i;
		}
		ctx->i++;
	}, &ctx);

	int lastFilterIndex = ctx.i + 1;
	if (newIndex > lastFilterIndex) {
		req->SendErrorResponse("index out of bounds");
		return;
	}

	int currentIndex = ctx.filterIndex;
	if (newIndex > currentIndex) {
		int downSteps = newIndex - currentIndex;
		for (int i = 0; i < downSteps; i++) {
			obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_DOWN);
		}
	}
	else if (newIndex < currentIndex) {
		int upSteps = currentIndex - newIndex;
		for (int i = 0; i < upSteps; i++) {
			obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_UP);
		}
	}

	req->SendOKResponse();
}

/**
* Move a filter in the chain (relative positioning)
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reorder
* @param {String} `movementType` How to move the filter around in the source's filter chain. Either "up", "down", "top" or "bottom".
*
* @api requests
* @name MoveSourceFilter
* @category sources
* @since 4.5.0
*/
void WSRequestHandler::HandleMoveSourceFilter(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("movementType")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	QString movementType(obs_data_get_string(req->data, "movementType"));

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_order_movement movement;
	if (movementType == "up") {
		movement = OBS_ORDER_MOVE_UP;
	}
	else if (movementType == "down") {
		movement = OBS_ORDER_MOVE_DOWN;
	}
	else if (movementType == "top") {
		movement = OBS_ORDER_MOVE_TOP;
	}
	else if (movementType == "bottom") {
		movement = OBS_ORDER_MOVE_BOTTOM;
	}
	else {
		req->SendErrorResponse("invalid value for movementType: must be either 'up', 'down', 'top' or 'bottom'.");
		return;
	}

	obs_source_filter_set_order(source, filter, movement);

	req->SendOKResponse();
}

/**
* Update settings of a filter
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reconfigure
* @param {Object} `filterSettings` New settings. These will be merged to the current filter settings.
*
* @api requests
* @name SetSourceFilterSettings
* @category sources
* @since 4.5.0
*/
void WSRequestHandler::HandleSetSourceFilterSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("filterSettings")) {
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	OBSDataAutoRelease newFilterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		req->SendErrorResponse("specified filter doesn't exist");
	}

	OBSDataAutoRelease settings = obs_source_get_settings(filter);
	obs_data_apply(settings, newFilterSettings);
	obs_source_update(filter, settings);

	req->SendOKResponse();
}
