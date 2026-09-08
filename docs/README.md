# 《人越来越多》网页图片维护说明

本文档用于说明如何给游戏展示网页添加新的轮播图片。

## 1. 准备图片

推荐使用以下规格：

- 图片比例：`16:9`
- 推荐尺寸：`1920 × 1080` 或 `2560 × 1440`
- 支持格式：`.jpg`、`.png`、`.webp`
- 推荐单张大小：不超过 `2 MB`

为了减少网页加载时间，优先使用经过压缩的 JPG 或 WebP 图片。

文件名建议只使用英文字母、数字和短横线，例如：

```text
level02-crowd.jpg
station-gate.webp
train-platform-03.png
```

尽量避免在文件名中使用空格、`#`、`?` 等特殊字符。

## 2. 将图片放入网页文件夹

把新图片复制到：

```text
docs/images/
```

例如：

```text
docs/images/level02-crowd.jpg
```

注意：GitHub Pages 和 EdgeOne 会区分文件名大小写。HTML 中填写的名称必须和实际文件名完全一致。

## 3. 在轮播中添加图片

打开：

```text
docs/index.html
```

搜索：

```html
<div class="carousel-track">
```

在这个容器中可以看到已有的多个 `<figure class="carousel-slide">`。在最后一张图片后、`carousel-track` 的结束标签前，添加下面的代码：

```html
<figure class="carousel-slide" aria-hidden="true">
  <img
    src="images/level02-crowd.jpg"
    alt="大量乘客穿过第二关站台"
    loading="lazy">

  <figcaption>
    <strong>这里填写图片标题</strong>
    <span>这里填写简短标签</span>
  </figcaption>
</figure>
```

需要修改的内容：

```text
images/level02-crowd.jpg       → 新图片的实际文件名
alt                           → 图片内容说明
这里填写图片标题               → 轮播左下角的大标题
这里填写简短标签               → 大标题下方的小标签
```

## 4. 不要修改的部分

新添加的图片必须保留：

```html
class="carousel-slide"
aria-hidden="true"
```

不要给新图片添加：

```html
is-active
```

`is-active` 只属于轮播中的第一张图片。否则网页加载时可能同时显示多张图片。

轮播页码、图片总数和底部导航条由 `script.js` 自动生成，不需要手动修改。

## 5. 完整示例

假设新增的文件是：

```text
docs/images/level03-train.jpg
```

对应代码为：

```html
<figure class="carousel-slide" aria-hidden="true">
  <img
    src="images/level03-train.jpg"
    alt="乘客在第三关进入指定列车车厢"
    loading="lazy">

  <figcaption>
    <strong>人数正确，车门才会打开</strong>
    <span>人数判定 / 精确登车</span>
  </figcaption>
</figure>
```

## 6. 本地检查

保存 `index.html` 后，双击打开：

```text
docs/index.html
```

检查：

- 新图片能否正常显示。
- 左右按钮能否切换到新图片。
- 底部导航数量是否自动增加。
- 图片标题和标签是否正确。
- 等待约 `4.8` 秒后是否自动切换。

如果图片显示为空白，优先检查：

1. 图片是否确实位于 `docs/images`。
2. `src` 中的文件名和扩展名是否正确。
3. 文件名大小写是否一致。
4. HTML 标签是否完整闭合。

## 7. 提交并发布

确认本地页面正常后，在项目根目录执行：

```powershell
git add docs
git commit -m "Add new gameplay screenshots"
git pull --rebase origin main
git push origin main
```

推送完成后，GitHub Pages 和 EdgeOne 会根据各自的项目配置重新部署。

如果线上页面暂时没有变化，请先检查对应平台的部署记录是否已经完成，然后强制刷新浏览器：

```text
Windows：Ctrl + F5
```

## 8. 删除或调整图片顺序

### 删除图片

从 `carousel-track` 中删除对应的完整 `<figure>...</figure>`，再删除不再使用的图片文件。

### 调整顺序

在 `carousel-track` 内移动完整的 `<figure>...</figure>` 代码块即可。

如果更换第一张图片，需要确保新的第一张保留：

```html
class="carousel-slide is-active"
aria-hidden="false"
```

其余图片应为：

```html
class="carousel-slide"
aria-hidden="true"
```
