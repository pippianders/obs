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
#include "display-helpers.hpp"
#include "qt-wrappers.hpp"

using namespace std;

Q_DECLARE_METATYPE(OBSSource);
Q_DECLARE_METATYPE(QuickTransition);

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

void OBSBasic::TransitionToScene(obs_source_t *source, bool force)
{
	obs_scene_t *scene = obs_scene_from_source(source);
	bool usingPreviewProgram = IsPreviewProgramMode();
	if (!scene)
		return;

	if (usingPreviewProgram) {
		scene = obs_scene_duplicate(scene, NULL,
				OBS_SCENE_DUP_PRIVATE_REFS);
		source = obs_scene_get_source(scene);
	}

	obs_source_t *transition = obs_get_output_source(0);

	if (force)
		obs_transition_set(transition, source);
	else
		obs_transition_start(transition, OBS_TRANSITION_MODE_AUTO,
				ui->transitionDuration->value(), source);

	obs_source_release(transition);

	if (usingPreviewProgram)
		obs_scene_release(scene);
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

void OBSBasic::SetQuickTransition(int idx)
{
}

void OBSBasic::SetCurrentScene(obs_scene_t *scene, bool force)
{
	obs_source_t *source = obs_scene_get_source(scene);
	SetCurrentScene(source, force);
}

void OBSBasic::SetCurrentScene(obs_source_t *scene, bool force)
{
	if (!IsPreviewProgramMode())
		TransitionToScene(scene, force);

	UpdateSceneSelection(scene);
}

void OBSBasic::CreateProgramDisplay()
{
	program = new OBSQTDisplay();

	auto displayResize = [this]() {
		struct obs_video_info ovi;

		if (obs_get_video_info(&ovi))
			ResizeProgram(ovi.base_width, ovi.base_height);
	};

	connect(program, &OBSQTDisplay::DisplayResized,
			displayResize);

	auto addDisplay = [this] (OBSQTDisplay *window)
	{
		obs_display_add_draw_callback(window->GetDisplay(),
				OBSBasic::RenderProgram, this);

		struct obs_video_info ovi;
		if (obs_get_video_info(&ovi))
			ResizeProgram(ovi.base_width, ovi.base_height);
	};

	connect(program, &OBSQTDisplay::DisplayCreated, addDisplay);

	program->setSizePolicy(QSizePolicy::Expanding,
			QSizePolicy::Expanding);
}

void OBSBasic::CreateProgramOptions()
{
	programOptions = new QWidget();
	QVBoxLayout *layout = new QVBoxLayout(programOptions);
	layout->setSpacing(4);

	QPushButton *transitionButton = new QPushButton(QTStr("Transitions"),
			programOptions);
	QHBoxLayout *quickTransitions = new QHBoxLayout(programOptions);
	quickTransitions->setSpacing(2);

	QPushButton *addQuickTransition = new QPushButton(programOptions);
	addQuickTransition->setMaximumSize(22, 22);
	addQuickTransition->setProperty("themeID", "addIconSmall");
	addQuickTransition->setFlat(true);

	QLabel *quickTransitionsLabel = new QLabel(QTStr("QuickTransitions"),
			programOptions);

	quickTransitions->addWidget(quickTransitionsLabel);
	quickTransitions->addWidget(addQuickTransition);

	layout->addStretch(0);
	layout->addWidget(transitionButton);
	layout->addLayout(quickTransitions);
	layout->addStretch(0);

	programOptions->setLayout(layout);

	auto transitionClicked = [this] () {
		TransitionToScene(GetCurrentScene());
	};

	connect(transitionButton, &QAbstractButton::clicked, transitionClicked);
}

void OBSBasic::on_modeSwitch_clicked()
{
	os_atomic_set_bool(&previewProgramMode, !IsPreviewProgramMode());

	if (IsPreviewProgramMode()) {
		CreateProgramDisplay();
		CreateProgramOptions();

		obs_scene_t *dup = obs_scene_duplicate(GetCurrentScene(),
				NULL, OBS_SCENE_DUP_PRIVATE_REFS);

		obs_source_t *transition = obs_get_output_source(0);
		obs_source_t *dup_source = obs_scene_get_source(dup);
		obs_transition_set(transition, dup_source);
		obs_source_release(transition);
		obs_scene_release(dup);

		ui->previewLayout->addWidget(programOptions);
		ui->previewLayout->addWidget(program);
		program->show();
	} else {
		TransitionToScene(GetCurrentScene(), true);

		delete programOptions;
		delete program;
	}
}

void OBSBasic::RenderProgram(void *data, uint32_t cx, uint32_t cy)
{
	OBSBasic *window = static_cast<OBSBasic*>(data);
	obs_video_info ovi;

	obs_get_video_info(&ovi);

	window->programCX = int(window->programScale * float(ovi.base_width));
	window->programCY = int(window->programScale * float(ovi.base_height));

	gs_viewport_push();
	gs_projection_push();

	/* --------------------------------------- */

	gs_ortho(0.0f, float(ovi.base_width), 0.0f, float(ovi.base_height),
			-100.0f, 100.0f);
	gs_set_viewport(window->programX, window->programY,
			window->programCX, window->programCY);

	window->DrawBackdrop(float(ovi.base_width), float(ovi.base_height));

	obs_render_main_view();
	gs_load_vertexbuffer(nullptr);

	/* --------------------------------------- */

	gs_projection_pop();
	gs_viewport_pop();

	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
}

void OBSBasic::ResizeProgram(uint32_t cx, uint32_t cy)
{
	QSize targetSize;

	/* resize program panel to fix to the top section of the window */
	targetSize = GetPixelSize(program);
	GetScaleAndCenterPos(int(cx), int(cy),
			targetSize.width()  - PREVIEW_EDGE_SIZE * 2,
			targetSize.height() - PREVIEW_EDGE_SIZE * 2,
			programX, programY, programScale);

	programX += float(PREVIEW_EDGE_SIZE);
	programY += float(PREVIEW_EDGE_SIZE);
}
