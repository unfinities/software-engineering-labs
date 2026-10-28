# 实验2 软件设计与建模

项目：校园二手交易系统。依据实验1冻结需求，新增设计，不改变实验1标签与提交包。

四种UML分别为必画类图、结构性组件图、行为性状态图、交互性时序图。`UML/`保留PlantUML源代码；`UI/`保留draw.io可编辑原稿；导出图片与个人报告在本地提交包中。

设计阶段采用模块化单体与关系数据库。数据库索引、请求记录、联系方式加密、失败日志补录属于乙方设计方案，不冒充甲方新增确认。性能环境仍待甲方确认。

本人已于2026-10-28完成三项设计核对并均保留，详见human-review.md。AI图稿修订与本人审查记录使用不同Git提交，不伪造人工改图；未创建lab2-final。

## 工具与重建

PlantUML 1.2026.9，draw.io Desktop 32.6.1。UML图用官方JAR本地渲染，UI图用官方draw.io命令导出；PNG已移除编辑器及本机元数据。

```console
java -Djava.awt.headless=true -jar plantuml.jar -charset UTF-8 -tpng lab2/UML/*.puml
drawio --export --format png --page-index 1 --width 1800 --theme light --output UI01.png lab2/UI/校园二手交易系统_UI.drawio
python scripts/check_lab2.py
```

依次使用1～14作为page-index导出所有UI。建议PLANTUML_LIMIT_SIZE=12000以防大图截边；公开图片应去除元数据后再提交。只下载官方工具，不把工具二进制或凭据纳入Git。


## 当前报告版本

本人已直接修改Word报告；当前匿名报告镜像已同步，具体变化见[报告人工修订说明](report-revision.md)及human-review.md的R2-04。个人Word是编辑源，保留本人排版；不要用早期生成器覆盖它。后续生成PDF与ZIP以该Word为准。
