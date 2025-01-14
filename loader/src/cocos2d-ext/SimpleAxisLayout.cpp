#include <Geode/ui/SimpleAxisLayout.hpp>

using namespace geode::prelude;

class SimpleAxisLayoutOptions::Impl {
public:
    std::optional<float> m_minRelativeScale = .5f;
    std::optional<float> m_maxRelativeScale = 2.f;
    ScalingPriority m_scalingPriority = ScalingPriority::Normal;
};

SimpleAxisLayoutOptions::SimpleAxisLayoutOptions() : m_impl(std::make_unique<Impl>()) {}
SimpleAxisLayoutOptions::~SimpleAxisLayoutOptions() = default;

SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::create() {
    auto ret = new SimpleAxisLayoutOptions();
    ret->autorelease();
    return ret;
}

SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::setMinRelativeScale(std::optional<float> scale) {
    m_impl->m_minRelativeScale = scale;
    return this;
}

SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::setMaxRelativeScale(std::optional<float> scale) {
    m_impl->m_maxRelativeScale = scale;
    return this;
}

SimpleAxisLayoutOptions* SimpleAxisLayoutOptions::setScalingPriority(ScalingPriority priority) {
    m_impl->m_scalingPriority = priority;
    return this;
}

std::optional<float> SimpleAxisLayoutOptions::getMinRelativeScale() const {
    return m_impl->m_minRelativeScale;
}

std::optional<float> SimpleAxisLayoutOptions::getMaxRelativeScale() const {
    return m_impl->m_maxRelativeScale;
}

ScalingPriority SimpleAxisLayoutOptions::getScalingPriority() const {
    return m_impl->m_scalingPriority;
}

class SimpleAxisLayout::Impl {
public:
    Axis m_axis = Axis::Column;
    AxisScaling m_mainAxisScaling = AxisScaling::None;
    AxisScaling m_crossAxisScaling = AxisScaling::None;
    MainAxisAlignment m_mainAxisAlignment = MainAxisAlignment::Start;
    CrossAxisAlignment m_crossAxisAlignment = CrossAxisAlignment::Center;
    AxisDirection m_mainAxisDirection = AxisDirection::FrontToBack;
    AxisDirection m_crossAxisDirection = AxisDirection::FrontToBack;
    float m_gap = 0.f;
    std::optional<float> m_minRelativeScale = 0.5f;
    std::optional<float> m_maxRelativeScale = 2.f;

    std::unordered_map<CCNode*, float> m_originalScalesPerNode;
    std::unordered_map<CCNode*, float> m_relativeScalesPerNode;

    Impl(Axis axis) : m_axis(axis) {
        switch (axis) {
            case Axis::Column:
                m_mainAxisDirection = AxisDirection::TopToBottom;
                m_crossAxisDirection = AxisDirection::LeftToRight;
                break;
            case Axis::Row:
                m_mainAxisDirection = AxisDirection::LeftToRight;
                m_crossAxisDirection = AxisDirection::TopToBottom;
                break;
        }
    }

    std::unordered_map<CCNode*, float> calculateCrossScaling(CCNode* layout, std::vector<CCNode*> const& nodes);
    std::unordered_map<CCNode*, float> calculateMainScaling(CCNode* layout, std::vector<CCNode*> const& nodes, float totalGap);
    
    void applyCrossPositioning(CCNode* layout, std::vector<CCNode*> const& nodes);
    void applyMainPositioning(CCNode* layout, std::vector<CCNode*> const& nodes, std::vector<SpacerNode*> const& spacers);

    void apply(cocos2d::CCNode* on);

    float getContentWidth(CCNode* on) const {
        if (m_axis == Axis::Column) {
            return on->getContentSize().width;
        }
        else {
            return on->getContentSize().height;
        }
    }

    float getContentHeight(CCNode* on) const {
        if (m_axis == Axis::Column) {
            return on->getContentSize().height;
        }
        else {
            return on->getContentSize().width;
        }
    }

    void setContentWidth(CCNode* on, float width) {
        if (m_axis == Axis::Column) {
            on->setContentSize({ width, on->getContentSize().height });
        }
        else {
            on->setContentSize({ on->getContentSize().width, width });
        }
    }

    void setContentHeight(CCNode* on, float height) {
        if (m_axis == Axis::Column) {
            on->setContentSize({ on->getContentSize().width, height });
        }
        else {
            on->setContentSize({ height, on->getContentSize().height });
        }
    }

    float getPositionX(CCNode* on) const {
        if (m_axis == Axis::Column) {
            return on->getPositionX();
        }
        else {
            return on->getPositionY();
        }
    }

    float getPositionY(CCNode* on) const {
        if (m_axis == Axis::Column) {
            return on->getPositionY();
        }
        else {
            return on->getPositionX();
        }
    }

    void setPositionX(CCNode* on, float x) {
        if (m_axis == Axis::Column) {
            on->setPositionX(x);
        }
        else {
            on->setPositionY(x);
        }
    }

    void setPositionY(CCNode* on, float y) {
        if (m_axis == Axis::Column) {
            on->setPositionY(y);
        }
        else {
            on->setPositionX(y);
        }
    }

    float getScale(CCNode* on) const {
        return on->getScale();
    }

    void setScale(CCNode* on, float scale) {
        on->setScale(scale);
    }

    SimpleAxisLayoutOptions* getLayoutOptions(CCNode* on) const {
        return typeinfo_cast<SimpleAxisLayoutOptions*>(on->getLayoutOptions());
    }

    std::optional<float> getMinScale(CCNode* on) const {
        auto const layoutOptions = this->getLayoutOptions(on);
        auto const minScale = layoutOptions ? layoutOptions->getMinRelativeScale() : m_minRelativeScale;
        if (minScale) return *minScale / m_relativeScalesPerNode.at(on);
        return std::nullopt;
    }

    std::optional<float> getMaxScale(CCNode* on) const {
        auto const layoutOptions = this->getLayoutOptions(on);
        auto const maxScale = layoutOptions ? layoutOptions->getMaxRelativeScale() : m_maxRelativeScale;
        if (maxScale) return *maxScale / m_relativeScalesPerNode.at(on);
        return std::nullopt;
    }

    float getMaxCrossScale(CCNode* layout, CCNode* on) const {
        auto const layoutWidth = this->getContentWidth(layout);
        auto const width = this->getContentWidth(on) * this->getScale(on);
        auto const maxAllowedScale = layoutWidth / width;
        auto const maxScale = this->getMaxScale(on);
        if (maxScale) return std::min(maxAllowedScale, *maxScale);
        return maxAllowedScale;
    }
};

// assumes scales are reverted before call
std::unordered_map<CCNode*, float> SimpleAxisLayout::Impl::calculateCrossScaling(CCNode* layout, std::vector<CCNode*> const& nodes) {
    std::unordered_map<CCNode*, float> scales;

    auto maxWidth = std::numeric_limits<float>::min();
    auto layoutWidth = this->getContentWidth(layout);

    // get the limits we are working with
    for (auto node : nodes) {
        auto const width = this->getContentWidth(node) * this->getScale(node);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }

    switch (m_crossAxisScaling) {
        case AxisScaling::Grow:
            if (maxWidth > layoutWidth) layoutWidth = maxWidth;
            break;
        case AxisScaling::Fit:
            layoutWidth = maxWidth;
            break;
        default:
            break;
    }

    this->setContentWidth(layout, layoutWidth);

    // get the scales we need for current limits
    for (auto node : nodes) {
        switch (m_crossAxisScaling) {
            case AxisScaling::ScaleDown: {
                auto const width = this->getContentWidth(node) * this->getScale(node);
                auto const minScale = this->getMinScale(node);

                if (width > layoutWidth) {
                    scales[node] = std::clamp(layoutWidth / width, minScale.value_or(0.f), 1.f);
                }
                break;
            }
            case AxisScaling::Scale: {
                auto const width = this->getContentWidth(node) * this->getScale(node);
                auto const minScale = this->getMinScale(node);
                auto const maxScale = this->getMaxScale(node);

                scales[node] = std::clamp(layoutWidth / width, minScale.value_or(0.f), maxScale.value_or(std::numeric_limits<float>::infinity()));
                break;
            }
            default:
                break;
        }
    }

    return scales;
}

// assumes scales are reverted before call
std::unordered_map<CCNode*, float> SimpleAxisLayout::Impl::calculateMainScaling(CCNode* layout, std::vector<CCNode*> const& nodes, float totalGap) {
    std::unordered_map<CCNode*, float> scales;

    auto totalHeight = totalGap;
    auto layoutHeight = this->getContentHeight(layout);

    // get the limits we are working with
    for (auto node : nodes) {
        auto const height = this->getContentHeight(node) * this->getScale(node);
        totalHeight += height;
    }

    switch (m_mainAxisScaling) {
        case AxisScaling::Grow:
            if (totalHeight > layoutHeight) layoutHeight = totalHeight;
            break;
        case AxisScaling::Fit:
            layoutHeight = totalHeight;
            break;
        default:
            break;
    }

    this->setContentHeight(layout, layoutHeight);

    std::unordered_map<ScalingPriority, std::vector<CCNode*>> sortedNodes;
    std::unordered_map<ScalingPriority, float> reducedHeightPerPriority;
    std::unordered_map<ScalingPriority, float> increasedHeightPerPriority;
    // calculate min max heights based on priorities
    for (auto node : nodes) {
        auto const layoutOptions = this->getLayoutOptions(node);
        auto const scalingPriority = layoutOptions ? layoutOptions->getScalingPriority() : ScalingPriority::Normal;
        sortedNodes[scalingPriority].push_back(node);

        switch (m_mainAxisScaling) {
            case AxisScaling::ScaleDown: {
                auto const height = this->getContentHeight(node) * this->getScale(node);
                auto const minScale = this->getMinScale(node);

                auto minHeight = height * minScale.value_or(0.f);
                reducedHeightPerPriority[scalingPriority] += height - minHeight;

                break;
            }
            case AxisScaling::Scale: {
                auto const height = this->getContentHeight(node) * this->getScale(node);
                auto const minScale = this->getMinScale(node);
                auto const maxScale = this->getMaxCrossScale(layout, node);

                auto minHeight = height * minScale.value_or(0.f);
                auto maxHeight = height * maxScale;
                reducedHeightPerPriority[scalingPriority] += height - minHeight;
                increasedHeightPerPriority[scalingPriority] += maxHeight - height;

                break;
            }
            default:
                break;
        }
    }

    if (m_mainAxisScaling != AxisScaling::ScaleDown && m_mainAxisScaling != AxisScaling::Scale) {
        return scales;
    }

    // sort the nodes by priority
    if (totalHeight > layoutHeight) {
        for (auto& [priority, sorted] : sortedNodes) {
            std::sort(sorted.begin(), sorted.end(), [&](CCNode* a, CCNode* b) {
                auto const prioA = this->getMinScale(a);
                auto const prioB = this->getMinScale(b);

                return prioA.value_or(0.f) > prioB.value_or(0.f);
            });
        }
    }
    else {
        for (auto& [priority, sorted] : sortedNodes) {
            std::sort(sorted.begin(), sorted.end(), [&](CCNode* a, CCNode* b) {
                auto const prioA = this->getMaxCrossScale(layout, a);
                auto const prioB = this->getMaxCrossScale(layout, b);

                return prioA < prioB;
            });
        }
    }

    for (auto priority : { 
        ScalingPriority::First, ScalingPriority::Early, ScalingPriority::Normal, 
        ScalingPriority::Late, ScalingPriority::Last 
    }) {
        if (totalHeight > layoutHeight) {
            // scale down
            auto const reducedHeight = reducedHeightPerPriority[priority];
            auto difference = totalHeight - layoutHeight;
            if (reducedHeight > difference) {
                // only partially scale down
                auto priorityHeight = 0.f;
                for (auto node : sortedNodes[priority]) {
                    auto const height = this->getContentHeight(node) * this->getScale(node);
                    priorityHeight += height;
                }
                auto remainingHeight = priorityHeight;

                auto targetScale = (remainingHeight - difference) / remainingHeight;
                // minScales are sorted in a decreasing priority
                for (auto node : sortedNodes[priority]) {
                    auto const height = this->getContentHeight(node) * this->getScale(node);
                    auto const minScale = this->getMinScale(node);

                    auto const scale = std::max(targetScale, minScale.value_or(0.f));
                    auto const minHeight = height * scale;
                    scales[node] = scale;

                    remainingHeight -= height;
                    difference -= height - minHeight;

                    // we need to readjust the target scale if we have remaining height
                    targetScale = (remainingHeight - difference) / remainingHeight;
                }

                break;
            }
            else {
                // scale down all the way
                for (auto node : sortedNodes[priority]) {
                    auto const minScale = this->getMinScale(node);
                    scales[node] = minScale.value_or(0.f);
                }
            }

            totalHeight -= reducedHeight;
        }
        else {
            if (m_mainAxisScaling == AxisScaling::ScaleDown) {
                break;
            }
            // scale up
            auto const increasedHeight = increasedHeightPerPriority[priority];
            auto difference = layoutHeight - totalHeight;
            if (increasedHeight > difference) {
                // only partially scale up
                auto priorityHeight = 0.f;
                for (auto node : sortedNodes[priority]) {
                    auto const height = this->getContentHeight(node) * this->getScale(node);
                    priorityHeight += height;
                }
                auto remainingHeight = priorityHeight;

                auto targetScale = (remainingHeight + difference) / remainingHeight;
                // maxScales are sorted in an increasing priority
                for (auto node : sortedNodes[priority]) {
                    auto const height = this->getContentHeight(node) * this->getScale(node);
                    auto const maxScale = this->getMaxCrossScale(layout, node);

                    auto const scale = std::min(targetScale, maxScale);
                    auto const maxHeight = height * scale;
                    scales[node] = scale;

                    remainingHeight -= height;
                    difference -= maxHeight - height;

                    // we need to readjust the target scale if we have remaining height
                    targetScale = (remainingHeight + difference) / remainingHeight;
                }

                break;
            }
            else {
                // scale up all the way
                for (auto node : sortedNodes[priority]) {
                    auto const maxScale = this->getMaxCrossScale(layout, node);
                    scales[node] = maxScale;
                }
            }
        }
    }

    return scales;
}

void SimpleAxisLayout::Impl::applyCrossPositioning(CCNode* layout, std::vector<CCNode*> const& nodes) {
    auto maxWidth = 0.f;
    auto layoutWidth = this->getContentWidth(layout);
    for (auto node : nodes) {
        auto const width = this->getContentWidth(node) * this->getScale(node);
        if (width > maxWidth) {
            maxWidth = width;
        }
    }

    switch (m_crossAxisScaling) {
        case AxisScaling::Grow:
            if (maxWidth > layoutWidth) layoutWidth = maxWidth;
            break;
        case AxisScaling::Fit:
            layoutWidth = maxWidth;
            break;
        default:
            break;
    }

    this->setContentWidth(layout, layoutWidth);

    CrossAxisAlignment alignment = m_crossAxisAlignment;
    if (m_crossAxisDirection == AxisDirection::BackToFront) {
        switch (m_crossAxisAlignment) {
            case CrossAxisAlignment::Start:
                alignment = CrossAxisAlignment::End;
                break;
            case CrossAxisAlignment::End:
                alignment = CrossAxisAlignment::Start;
                break;
            default:
                break;
        }
    }

    for (auto node : nodes) {
        auto const layoutOptions = this->getLayoutOptions(node);
        auto const scale = this->getScale(node);
        auto const width = this->getContentWidth(node) * scale;
        auto const remainingWidth = layoutWidth - width;

        node->setAnchorPoint({ 0.5f, 0.5f });

        switch (alignment) {
            case CrossAxisAlignment::Start:
                this->setPositionX(node, width * 0.5f);
                break;
            case CrossAxisAlignment::Center:
                this->setPositionX(node, remainingWidth * 0.5f + width * 0.5f);
                break;
            case CrossAxisAlignment::End:
                this->setPositionX(node, remainingWidth + width * 0.5f);
                break;
            default:
                break;
        }
    }
}
void SimpleAxisLayout::Impl::applyMainPositioning(CCNode* layout, std::vector<CCNode*> const& nodes, std::vector<SpacerNode*> const& spacers) {
    auto totalHeight = 0.f;
    for (auto node : nodes) {
        auto const height = this->getContentHeight(node) * this->getScale(node);
        totalHeight += height;
    }
    auto const layoutHeight = this->getContentHeight(layout);
    auto const remainingHeight = layoutHeight - totalHeight;

    auto extraGap = remainingHeight;
    auto offset = 0.f;
    auto spacerGap = 0.f;
    if (spacers.size() > 0) {
        size_t totalGrow = 0;
        for (auto spacer : spacers) {
            totalGrow += spacer->getGrow();
        }
        extraGap = 0.f;
        offset = 0.f;
        spacerGap = remainingHeight / totalGrow;
    }
    else {
        switch (m_mainAxisAlignment) {
            case MainAxisAlignment::Start:
                extraGap = 0.f;
                offset = 0.f;
                break;
            case MainAxisAlignment::Center:
                extraGap = 0.f;
                offset = remainingHeight / 2;
                break;
            case MainAxisAlignment::End:
                extraGap = 0.f;
                offset = remainingHeight;
                break;
            case MainAxisAlignment::Even:
                extraGap = extraGap / (nodes.size() + 1);
                offset = extraGap;
                break;
            case MainAxisAlignment::Between:
                extraGap = extraGap / (nodes.size() - 1);
                offset = 0.0f;
                break;
            case MainAxisAlignment::Around:
                extraGap = extraGap / nodes.size();
                offset = extraGap / 2.0f;
                break;
        }
    }

    if (m_mainAxisDirection == AxisDirection::BackToFront) {
        offset = layoutHeight - offset;
    }

    CCNode* lastChild = nullptr;
    for (auto node : nodes) {
        if (auto gap = typeinfo_cast<AxisGap*>(node)) {
            offset += gap->getGap();
            lastChild = nullptr;
            continue;
        }
        if (lastChild) {
            offset += m_gap;
        }

        auto const height = this->getContentHeight(node) * this->getScale(node);
        
        node->ignoreAnchorPointForPosition(false);
        node->setAnchorPoint(ccp(0.5f, 0.5f));

        switch (m_mainAxisDirection) {
            case AxisDirection::BackToFront:
                this->setPositionY(node, offset - height / 2);
                offset -= height + extraGap;
                break;
            case AxisDirection::FrontToBack:
                this->setPositionY(node, offset + height / 2);
                offset += height + extraGap;
                break;
        }
        lastChild = node;
    }
}

void SimpleAxisLayout::Impl::apply(cocos2d::CCNode* layout) {
    std::vector<CCNode*> realChildren;
    std::vector<CCNode*> positionChildren;
    std::vector<SpacerNode*> spacers;
    std::vector<AxisGap*> gaps;
    float totalGap = 0.f;
    CCNode* lastChild = nullptr;
    for (auto child : CCArrayExt<CCNode*>(layout->getChildren())) {
        if (auto spacer = typeinfo_cast<SpacerNode*>(child)) {
            spacers.push_back(spacer);
        }
        else if (auto gap = typeinfo_cast<AxisGap*>(child)) {
            gaps.push_back(gap);
            totalGap += gap->getGap();
            switch (m_mainAxisAlignment) {
                case MainAxisAlignment::Start:
                case MainAxisAlignment::Center:
                case MainAxisAlignment::End:
                    positionChildren.push_back(gap);
                default:
                    break;
            }
            lastChild = nullptr;
        }
        else {
            if (lastChild) {
                totalGap += m_gap;
            }
            realChildren.push_back(child);
            positionChildren.push_back(child);
            lastChild = child;
        }
    }

    // revert back to original scale
    for (auto child : realChildren) {
        auto const expectedScale = m_originalScalesPerNode[child] * m_relativeScalesPerNode[child];
        auto const scale = this->getScale(child);
        if (scale != expectedScale) {
            m_originalScalesPerNode[child] = scale;
        }
        else {
            this->setScale(child, m_originalScalesPerNode[child]);
        }
        m_relativeScalesPerNode[child] = 1.f;
    }

    // calculate required max scales
    auto crossScales = this->calculateCrossScaling(layout, realChildren);
    for (auto child : realChildren) {
        auto scale = 1.f;
        if (crossScales.contains(child)) {
            scale *= crossScales[child];
        }

        this->setScale(child, scale);
        m_relativeScalesPerNode[child] = scale;
    }

    auto mainScales = this->calculateMainScaling(layout, realChildren, totalGap);

    for (auto child : realChildren) {
        auto scale = m_relativeScalesPerNode[child];
        if (mainScales.contains(child)) {
            scale *= mainScales[child];
        }

        this->setScale(child, scale);
        m_relativeScalesPerNode[child] = scale;
    }

    // apply positions
    this->applyCrossPositioning(layout, realChildren);
    this->applyMainPositioning(layout, positionChildren, spacers);
}

SimpleAxisLayout::SimpleAxisLayout(Axis axis) : m_impl(std::make_unique<Impl>(axis)) {}

SimpleAxisLayout::~SimpleAxisLayout() = default;

SimpleAxisLayout* SimpleAxisLayout::create(Axis axis) {
    auto ret = new SimpleAxisLayout(axis);
    ret->autorelease();
    return ret;
}

cocos2d::CCSize SimpleAxisLayout::getSizeHint(cocos2d::CCNode* on) const {
    return on->getContentSize();
}

void SimpleAxisLayout::apply(cocos2d::CCNode* on) {
    m_impl->apply(on);
}

SimpleAxisLayout* SimpleAxisLayout::setAxis(Axis axis) {
    m_impl->m_axis = axis;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setMainAxisScaling(AxisScaling scaling) {
    m_impl->m_mainAxisScaling = scaling;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setCrossAxisScaling(AxisScaling scaling) {
    m_impl->m_crossAxisScaling = scaling;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setMainAxisAlignment(MainAxisAlignment alignment) {
    m_impl->m_mainAxisAlignment = alignment;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setCrossAxisAlignment(CrossAxisAlignment alignment) {
    m_impl->m_crossAxisAlignment = alignment;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setMainAxisDirection(AxisDirection direction) {
    m_impl->m_mainAxisDirection = direction;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setCrossAxisDirection(AxisDirection direction) {
    m_impl->m_crossAxisDirection = direction;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setGap(float gap) {
    m_impl->m_gap = gap;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setMinRelativeScale(std::optional<float> scale) {
    m_impl->m_minRelativeScale = scale;
    return this;
}

SimpleAxisLayout* SimpleAxisLayout::setMaxRelativeScale(std::optional<float> scale) {
    m_impl->m_maxRelativeScale = scale;
    return this;
}

SimpleRowLayout::SimpleRowLayout() : SimpleAxisLayout(Axis::Row) {}

SimpleRowLayout::~SimpleRowLayout() = default;

SimpleRowLayout* SimpleRowLayout::create() {
    auto ret = new SimpleRowLayout();
    ret->autorelease();
    return ret;
}

SimpleColumnLayout::SimpleColumnLayout() : SimpleAxisLayout(Axis::Column) {}

SimpleColumnLayout::~SimpleColumnLayout() = default;

SimpleColumnLayout* SimpleColumnLayout::create() {
    auto ret = new SimpleColumnLayout();
    ret->autorelease();
    return ret;
}

class AxisGap::Impl {
public:
    float m_gap;

    Impl(float gap) : m_gap(gap) {}
};

AxisGap::AxisGap(float gap) : m_impl(std::make_unique<Impl>(gap)) {}

AxisGap::~AxisGap() = default;

AxisGap* AxisGap::create(float gap) {
    auto ret = new AxisGap(gap);
    ret->autorelease();
    return ret;
}

float AxisGap::getGap() const {
    return m_impl->m_gap;
}

AxisGap* AxisGap::setGap(float gap) {
    m_impl->m_gap = gap;
    return this;
}