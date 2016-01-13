/******************************************************************************
    Copyright (C) 2016 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"

using namespace std;

Q_DECLARE_METATYPE(OBSSource);

void OBSBasic::InitDefaultTransitions()
{
	size_t idx = 0;
	const char *id;

	/* automatically add transitions that have no configuration (things
	 * such as cut/fade/etc) */
	while (obs_enum_transition_types(idx++, &id)) {
		if (!obs_is_source_configurable(id)) {
			const char *name = obs_source_get_display_name(id);

			obs_source_t *tr = obs_source_create_private(
					id, name, NULL);
			transitions.emplace_back(tr);

			if (strcmp(id, "fade_transition") == 0)
				fadeTransition = tr;

			obs_source_release(tr);
		}
	}

	for (OBSSource &tr : transitions) {
		ui->transitions->addItem(QT_UTF8(obs_source_get_name(tr)),
				QVariant::fromValue(OBSSource(tr)));
	}
}

obs_source_t *OBSBasic::FindTransition(const char *name)
{
	for (OBSSource &tr : transitions) {
		const char *trName = obs_source_get_name(tr);
		if (strcmp(trName, name) == 0)
			return tr;
	}

	return nullptr;
}

void OBSBasic::TransitionToScene(obs_scene_t *scene, bool force)
{
	obs_source_t *source = obs_scene_get_source(scene);
	TransitionToScene(source, force);
}

void OBSBasic::TransitionToScene(obs_source_t *scene, bool force)
{
	obs_source_t *transition = obs_get_output_source(0);

	if (force)
		obs_transition_set(transition, scene);
	else
		obs_transition_start(transition, OBS_TRANSITION_MODE_AUTO,
				ui->transitionDuration->value(), scene);

	UpdateSceneSelection(scene);
	obs_source_release(transition);
}

static inline void SetComboTransition(QComboBox *combo, obs_source_t *tr)
{
	int idx = combo->findData(QVariant::fromValue<OBSSource>(tr));
	if (idx != -1) {
		combo->blockSignals(true);
		combo->setCurrentIndex(idx);
		combo->blockSignals(false);
	}
}

void OBSBasic::SetTransition(obs_source_t *transition)
{
	obs_source_t *oldTransition = obs_get_output_source(0);

	if (oldTransition) {
		obs_transition_swap_begin(transition, oldTransition);
		if (transition != GetCurrentTransition())
			SetComboTransition(ui->transitions, transition);
		obs_set_output_source(0, transition);
		obs_transition_swap_end(transition, oldTransition);

		bool showPropertiesButton = obs_source_configurable(transition);
		ui->transitionProps->setVisible(showPropertiesButton);

		obs_source_release(oldTransition);
	} else {
		obs_set_output_source(0, transition);
	}
}

OBSSource OBSBasic::GetCurrentTransition()
{
	return ui->transitions->currentData().value<OBSSource>();
}

void OBSBasic::on_transitions_currentIndexChanged(int)
{
	SetTransition(GetCurrentTransition());
}

void OBSBasic::on_transitionProps_clicked()
{
	// TODO
}
