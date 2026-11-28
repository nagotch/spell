// spell ライブラリのサイト内検索。
// 生成された静的インデックス search-index.json を読み、名前・メソッド名・
// 概要・キーワードに対して、空白区切りの全トークンの部分一致で絞り込む。
// サーバー側の処理は無く、このファイルとインデックスだけで動く。
(function () {
  var boxes = document.querySelectorAll("[data-lib-search]");
  if (!boxes.length) return;

  function wire(box, index) {
    var input = box.querySelector("input");
    var list = box.querySelector(".lib-search-results");
    if (!input || !list) return;

    function render() {
      var tokens = input.value.toLowerCase().split(/\s+/).filter(function (t) {
        return t.length > 0;
      });
      list.innerHTML = "";
      if (!tokens.length) {
        box.classList.remove("active");
        return;
      }
      var hits = index.filter(function (e) {
        var hay = (e.t + " " + e.w + " " + e.s).toLowerCase();
        return tokens.every(function (t) { return hay.indexOf(t) >= 0; });
      }).slice(0, 30);
      box.classList.add("active");
      if (!hits.length) {
        var none = document.createElement("li");
        none.className = "none";
        none.textContent = "見つかりませんでした";
        list.appendChild(none);
        return;
      }
      hits.forEach(function (e) {
        var li = document.createElement("li");
        var a = document.createElement("a");
        a.href = e.u;
        var kind = document.createElement("span");
        kind.className = "rk";
        kind.textContent = e.k;
        var name = document.createElement("span");
        name.className = "rn";
        name.textContent = e.t;
        var sum = document.createElement("span");
        sum.className = "rs";
        sum.textContent = e.s;
        a.appendChild(kind);
        a.appendChild(name);
        a.appendChild(sum);
        li.appendChild(a);
        list.appendChild(li);
      });
    }

    input.addEventListener("input", render);
    render();
  }

  fetch("search-index.json")
    .then(function (res) { return res.json(); })
    .then(function (index) {
      boxes.forEach(function (box) { wire(box, index); });
    })
    .catch(function () {
      boxes.forEach(function (box) {
        var input = box.querySelector("input");
        if (input) input.placeholder = "検索インデックスを読み込めませんでした";
      });
    });
})();
